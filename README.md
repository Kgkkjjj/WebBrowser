# Multi-language Web Scraper

This repository provides simple example programs that fetch data from ten free APIs using Python, Go, Node.js, and C. The APIs require no authentication keys and demonstrate basic web scraping across different languages.

## APIs

The programs access the following endpoints:

1. `https://api.publicapis.org/entries`
2. `https://dog.ceo/api/breeds/image/random`
3. `https://catfact.ninja/fact`
4. `https://api.agify.io?name=John`
5. `https://api.genderize.io?name=John`
6. `https://api.nationalize.io?name=John`
7. `https://official-joke-api.appspot.com/jokes/random`
8. `https://www.boredapi.com/api/activity`
9. `https://api.coindesk.com/v1/bpi/currentprice.json`
10. `https://api.ipify.org?format=json`

## Python

Run the Python scraper:

```bash
python python_scraper.py
```

## Node.js

Run the Node.js scraper:

```bash
node node_scraper.js
```

## Go

Build and run the Go scraper:

```bash
go build go_scraper.go
./go_scraper
```

## C

Compile and run the C scraper (requires libcurl):

```bash
gcc c_scraper.c -o c_scraper -lcurl
./c_scraper
```

## Requirements

- Python 3 with `requests`
- Node.js
- Go toolchain
- GCC and libcurl

No API keys are necessary to access the listed endpoints.
