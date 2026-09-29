# HAL conversation providers

HAL always resolves the local, allow-listed media commands before sending any
text to an AI provider. AI responses are text-only and cannot execute desktop
commands.

## Local Ollama (default)

DarkSpark uses Ollama's OpenAI-compatible Responses endpoint on localhost.
After Ollama is installed and running, download the default model:

```bash
ollama pull qwen3:4b
```

Then launch DarkSpark normally. The defaults are equivalent to:

```bash
export DARKSPARK_AI_PROVIDER=ollama
export DARKSPARK_AI_MODEL=qwen3:4b
export DARKSPARK_AI_ENDPOINT=http://127.0.0.1:11434/v1/responses
```

Nothing spoken to the local provider leaves the computer.

## OpenAI API

Set the provider and API key in the environment that launches DarkSpark:

```bash
export DARKSPARK_AI_PROVIDER=openai
export DARKSPARK_AI_MODEL=gpt-5.6-luna
export OPENAI_API_KEY='replace-with-your-project-key'
```

Do not commit API keys to this repository or place them in command-line
arguments. `DARKSPARK_AI_ENDPOINT` can override the endpoint for testing or a
compatible gateway.

## Disable conversation

```bash
export DARKSPARK_AI_PROVIDER=none
```

Voice-controlled media actions continue to work when conversation is disabled.
