
import os
import io
import urllib.parse

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import Response
from openai import OpenAI
from dotenv import load_dotenv
load_dotenv()

app = FastAPI(title="ESP32 Voice Assistant")

client = OpenAI(
    api_key=os.environ.get("OPENAI_API_KEY")
)


@app.get("/")
def home():
    return {
        "status": "online",
        "service": "ESP32 Voice Assistant"
    }


@app.post("/api/voice")
async def process_voice(request: Request):

    audio_data = await request.body()

    if not audio_data:
        raise HTTPException(
            status_code=400,
            detail="No audio received"
        )

    if len(audio_data) > 200000:
        raise HTTPException(
            status_code=413,
            detail="Audio is too large"
        )

    try:

        # Convert raw PCM audio to WAV.
        import wave

        wav_buffer = io.BytesIO()

        with wave.open(wav_buffer, "wb") as wav_file:

            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(16000)

            wav_file.writeframes(audio_data)

        wav_buffer.seek(0)

        wav_buffer.name = "recording.wav"

        # Speech-to-text
        transcription = client.audio.transcriptions.create(
            model="gpt-transcribe",
            file=wav_buffer
        )

        user_text = transcription.text

        if not user_text.strip():
            raise HTTPException(
                status_code=400,
                detail="No speech detected"
            )

        # Generate AI response
        completion = client.responses.create(
            model=os.getenv("OPENAI_CHAT_MODEL", "gpt-5.5"),
            instructions=(
                "You are a helpful voice assistant. "
                "Respond naturally and concisely. "
                "Your response will be spoken aloud."
            ),
            input=user_text
        )

        answer = completion.output_text

        # Generate speech
        speech = client.audio.speech.create(
            model="gpt-4o-mini-tts",
            voice="marin",
            input=answer,
            response_format="pcm"
        )

        encoded_answer = urllib.parse.quote(answer, safe="")

        return Response(
            content=speech.content,
            media_type="application/octet-stream",
            headers={
                "X-Reply-Text": encoded_answer
            }
        )

    except HTTPException:
        raise

    except Exception as error:

        print("Voice processing error:", error)

        raise HTTPException(
            status_code=500,
            detail="Voice processing failed"
        )
