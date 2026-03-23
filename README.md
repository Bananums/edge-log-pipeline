# edge-log-pipeline
Just a playground repo to test out Grafana for edge log pipeline

## Structure ##

```text
.
├── app/                          # C++ quill app (your existing code)
│   ├── BUILD.bazel
│   └── src/
│
├── infra/
│   ├── observability/            # The observability stack
│   │   ├── compose.yml           # service wiring
│   │   ├── .env.example          # committed — documents required vars
│   │   ├── .env                  # gitignored — actual secrets/local values
│   │   │
│   │   ├── alloy/
│   │   │   └── config.alloy
│   │   │
│   │   ├── loki/
│   │   │   └── config.yaml
│   │   │
│   │   └── grafana/
│   │       ├── provisioning/     # auto-wires datasources + dashboards
│   │       │   ├── datasources/
│   │       │   │   └── loki.yaml
│   │       │   └── dashboards/
│   │       │       └── default.yaml
│   │       └── dashboards/       # dashboard JSON lives here
│   │
│   └── README.md                 # how to deploy to VPS
│
└── third_party/
```

## Loki http checks ##

* http://localhost:3100/ready: The health check. Returns plain text ready when Loki is fully initialized.
* http://localhost:3100/metrics: Exposes Prometheus-style metrics about Loki itself. Ingestion rate, query performance, memory usage etc.
* http://localhost:3100/config: Returns the full resolved config Loki is actually running with. Very useful for debugging shows if config.yaml was parsed correctly and shows all the defaults Loki filled in automatically.
* http://localhost:3100/loki/api/v1/labels: Returns all log labels Loki currently knows about. Right now it wil. return an empty result since no logs have been pushed yet — but this is a good one to bookmark for when Alloy is running.
