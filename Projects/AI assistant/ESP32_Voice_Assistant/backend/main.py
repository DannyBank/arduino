import os
import io
import wave
import logging
from urllib.parse import quote

from dotenv import load_dotenv
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import Response, JSONResponse
from starlette.concurrency import run_in_threadpool

import anthropic
from openai import OpenAI


# ---------------------------------------------------------
# ENVIRONMENT AND LOGGING
# ---------------------------------------------------------

load_dotenv()

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)s | %(message)s"
)

logger = logging.getLogger("esp32-voice-assistant")


ANTHROPIC_API_KEY = os.getenv("ANTHROPIC_API_KEY")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")

# Models configuration
WHISPER_MODEL = os.getenv("WHISPER_MODEL", "whisper-1")
CLAUDE_MODEL = os.getenv("CLAUDE_MODEL", "claude-3-5-haiku-20241022")
TTS_MODEL = os.getenv("TTS_MODEL", "tts-1")
VOICE_NAME = os.getenv("VOICE_NAME", "alloy")

# ESP32 microphone audio format
MIC_SAMPLE_RATE = 16000
MIC_CHANNELS = 1
MIC_SAMPLE_WIDTH = 2  # 16-bit audio

# TTS output format (24kHz 16-bit mono PCM)
TTS_SAMPLE_RATE = 24000
TTS_CHANNELS = 1
TTS_SAMPLE_WIDTH = 2  # 16-bit audio


if not ANTHROPIC_API_KEY:
    raise RuntimeError("ANTHROPIC_API_KEY is missing. Add it to your .env file.")

if not OPENAI_API_KEY:
    raise RuntimeError("OPENAI_API_KEY is missing (needed for Whisper transcription & OpenAI TTS).")


# ---------------------------------------------------------
# CLIENT INITIALIZATIONS
# ---------------------------------------------------------

claude_client = anthropic.Anthropic(api_key=ANTHROPIC_API_KEY)
openai_client = OpenAI(api_key=OPENAI_API_KEY)


# ---------------------------------------------------------
# FASTAPI APPLICATION
# ---------------------------------------------------------

app = FastAPI(
    title="ESP32 Claude Voice Assistant",
    description="Cloud AI backend for an ESP32 voice assistant using Whisper, Claude, and OpenAI TTS",
    version="3.0.0"
)


# ---------------------------------------------------------
# HELPER: CHECK WHETHER ERROR IS RATE LIMIT OR SERVICE UNAVAILABLE
# ---------------------------------------------------------

def is_transient_api_error(error: Exception) -> bool:
    """
    Detect HTTP 429 (rate limit) or HTTP 503 (service unavailable) errors.
    """
    status_code = getattr(error, "status_code", None)
    if status_code in (429, 503):
        return True

    error_message = str(error).lower()
    return (
        "429" in error_message
        or "503" in error_message
        or "rate limit" in error_message
        or "service_unavailable" in error_message
    )


# ---------------------------------------------------------
# HELPER: CONVERT RAW PCM TO WAV
# ---------------------------------------------------------

def pcm_to_wav(
    pcm_data: bytes,
    sample_rate: int,
    channels: int = 1,
    sample_width: int = 2
) -> bytes:

    wav_buffer = io.BytesIO()

    with wave.open(wav_buffer, "wb") as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(pcm_data)

    return wav_buffer.getvalue()


# ---------------------------------------------------------
# STEP 1: TRANSCRIBE WITH WHISPER & GET CLAUDE RESPONSE
# ---------------------------------------------------------

def generate_ai_response(audio_wav: bytes) -> str:
    """
    Transcribe WAV audio using OpenAI Whisper API, then pass text to Claude.
    """
    audio_file = io.BytesIO(audio_wav)
    audio_file.name = "audio.wav"

    logger.info("Sending audio to OpenAI Whisper model: %s", WHISPER_MODEL)

    transcript_response = openai_client.audio.transcriptions.create(
        model=WHISPER_MODEL,
        file=audio_file,
        response_format="text"
    )

    user_text = (
        transcript_response.strip()
        if isinstance(transcript_response, str)
        else transcript_response.text.strip()
    )

    logger.info("Whisper Transcript: %s", user_text)

    if not user_text:
        return "I didn't catch that. Could you please repeat yourself?"

    system_prompt = """
You are a helpful, friendly voice assistant running through an ESP32 device.

Respond to what the user said.

Instructions:
- Respond naturally, as if having a spoken conversation.
- Keep answers concise and easy to understand.
- Avoid markdown, bullet points, and unnecessary formatting.
- Do not describe the audio unless the user asks you to.
- If the transcript is unclear or nonsensical, politely ask the user to repeat themselves.
- Do not invent information.
"""

    logger.info("Sending transcript to Claude model: %s", CLAUDE_MODEL)

    message = claude_client.messages.create(
        model=CLAUDE_MODEL,
        max_tokens=200,
        system=system_prompt,
        messages=[
            {"role": "user", "content": user_text}
        ]
    )

    answer = message.content[0].text

    if not answer:
        raise RuntimeError("Claude returned an empty text response.")

    logger.info("Claude responded successfully.")
    return answer.strip()


# ---------------------------------------------------------
# STEP 2: CONVERT TEXT RESPONSE TO SPEECH
# ---------------------------------------------------------

def generate_speech(text: str) -> bytes:
    """
    Convert text response into mono 16-bit PCM audio using OpenAI TTS.
    """
    logger.info("Generating speech using model: %s (voice: %s)", TTS_MODEL, VOICE_NAME)

    response = openai_client.audio.speech.create(
        model=TTS_MODEL,
        voice=VOICE_NAME,
        input=text,
        response_format="pcm"
    )

    audio_data = response.content

    if not audio_data:
        raise RuntimeError("TTS returned empty audio.")

    logger.info("TTS generated %s bytes of audio.", len(audio_data))
    return audio_data


# ---------------------------------------------------------
# HEALTH CHECK
# ---------------------------------------------------------

@app.get("/")
async def root():
    return {
        "status": "online",
        "service": "ESP32 Claude Voice Assistant",
        "whisper_model": WHISPER_MODEL,
        "claude_model": CLAUDE_MODEL,
        "tts_model": TTS_MODEL
    }


@app.get("/health")
async def health():
    return {
        "status": "healthy"
    }


# ---------------------------------------------------------
# MAIN ESP32 VOICE ENDPOINT
# ---------------------------------------------------------

@app.post("/api/voice")
async def process_voice(request: Request):
    try:
        audio_pcm = await request.body()
        audio_size = len(audio_pcm)

        logger.info("Received %s bytes of audio from ESP32.", audio_size)

        if audio_size == 0:
            raise HTTPException(
                status_code=400,
                detail="No audio data received."
            )

        if audio_size % MIC_SAMPLE_WIDTH != 0:
            raise HTTPException(
                status_code=400,
                detail="Audio length is invalid for 16-bit PCM."
            )

        # Convert PCM to WAV
        audio_wav = pcm_to_wav(
            pcm_data=audio_pcm,
            sample_rate=MIC_SAMPLE_RATE,
            channels=MIC_CHANNELS,
            sample_width=MIC_SAMPLE_WIDTH
        )

        logger.info("Converted audio to WAV. WAV size: %s bytes.", len(audio_wav))

        # Transcribe via Whisper and get Claude LLM response
        logger.info("Processing audio with Whisper & Claude...")
        reply_text = await run_in_threadpool(
            generate_ai_response,
            audio_wav
        )
        logger.info("AI reply: %s", reply_text)

        # Convert response text to speech audio
        logger.info("Converting response to speech...")
        speech_pcm = await run_in_threadpool(
            generate_speech,
            reply_text
        )

        encoded_reply = quote(reply_text, safe="")

        logger.info("Returning %s bytes of speech audio to ESP32.", len(speech_pcm))

        return Response(
            content=speech_pcm,
            media_type="application/octet-stream",
            headers={
                "X-Reply-Text": encoded_reply,
                "X-Audio-Sample-Rate": str(TTS_SAMPLE_RATE),
                "X-Audio-Channels": str(TTS_CHANNELS),
                "X-Audio-Bits": "16"
            }
        )

    except HTTPException:
        raise

    except Exception as error:
        logger.exception("Voice processing failed: %s", error)

        status_code = 503 if is_transient_api_error(error) else 500

        return JSONResponse(
            status_code=status_code,
            content={
                "error": "Voice processing failed",
                "message": str(error)
            }
        )


# ---------------------------------------------------------
# RUN DIRECTLY (OPTIONAL)
# ---------------------------------------------------------

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True
    )