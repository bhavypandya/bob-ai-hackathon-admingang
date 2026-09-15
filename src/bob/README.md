# Bob Integration Guide

## Overview

Bob serves as the real-time AI operations assistant for the Supply Chain Disruption Assistant.
Bob communicates directly with the C++ backend REST API to retrieve live operational data
and generates actionable responses.

## How It Works

```
User asks Bob a question
        ↓
Bob calls POST /api/bob/query with the question
        ↓
C++ backend analyses the question using keyword matching
        ↓
Backend queries live SQLite data (disruptions, shipments, fleet, cold-chain)
        ↓
Backend generates a structured, data-driven answer
        ↓
Bob presents the answer to the user
```

## API Endpoint

```
POST http://localhost:8080/api/bob/query
Content-Type: application/json

{
  "question": "What should we do right now?"
}
```

## Response Format

```json
{
  "question": "What should we do right now?",
  "answer": "Current situation:\n\n3 active disruption(s):\n..."
}
```

## Questions Bob Can Answer

| Question | API Response |
|---|---|
| "What should we do right now?" | Full operational summary + top 5 actions |
| "Which shipments are affected?" | List with risk levels |
| "Highest risk shipment?" | Top shipment + reasons |
| "Best alternative route?" | Route recommendation with comparison |
| "Should we change the carrier?" | Carrier recommendation with reasoning |
| "Which fleet assets are idle?" | Idle asset list with locations |
| "Where should we redeploy?" | Redeployment opportunities |
| "Cold-chain at risk?" | All cold-chain alerts with severity |
| "Disruption summary" | Full disruption overview |

## Running Bob

1. Start the backend:
   ```bash
   src/build/bin/supply_chain_backend.exe
   ```

2. Open the dashboard at `http://localhost:8080`

3. Click the **🤖 Bob AI** tab

4. Use quick-question buttons or type any operational question

## Integration Notes

- Bob reads data directly from the running C++ backend
- All values are from live application state (not hardcoded)
- Cold-chain severity is clearly labelled "Configured severity"
- All recommendations include explainable reasons

## Extending Bob

To add new question types, edit [`src/backend/api/api_handler.cpp`](../backend/api/api_handler.cpp)
in the `handleBobQuery` function — add new keyword patterns and corresponding data queries.
