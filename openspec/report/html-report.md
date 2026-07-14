# HTML Report Rendering

## Facts

- HTML template is embedded at compile time with `INCBIN(templateHtml, "./templates/template.html")` (`main.cpp:52`).
- Main builds an `inja::Environment`, registers callback `objectToArray`, renders template against `reportGenerator->getJson()`, and writes `report.html` (`main.cpp:286` through `main.cpp:344`).
- Template consumes keys from `metadata`, `grandTotal`, `processes`, `data`, optional `pssByGroup`, optional `cpuIdleStats` (`templates/template.html`).

## Inja/JSON Usage Patterns

### Facts - Inja Usage

- `_columnOrder` plus callback `objectToArray` is used to provide deterministic table ordering in HTML despite unordered JSON objects (`main.cpp:294` through `main.cpp:324`, `JsonReportGenerator.cpp:53`).
- Process table and dataset tables are rendered from arrays and enhanced with DataTables JS in the template (`templates/template.html`).

## Inferences

- The HTML report is intended as interactive triage UI with sort/filter/export capability; machine ingestion should rely on JSON.

## Unknowns

- Runtime access to remote CDN assets (Bootstrap/DataTables/Chart.js) may vary by deployment environment.
