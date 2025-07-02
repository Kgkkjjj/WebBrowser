# Unified Web Scraper

This repository now contains a single Python program that fetches data from thirty free APIs. It consolidates the previous C, Go, and Node.js examples into one script.

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
11. `https://randomuser.me/api/`
12. `https://api.adviceslip.com/advice`
13. `https://api.kanye.rest/`
14. `https://yesno.wtf/api`
15. `https://api.chucknorris.io/jokes/random`
16. `https://ron-swanson-quotes.herokuapp.com/v2/quotes`
17. `http://api.open-notify.org/iss-now.json`
18. `https://randomfox.ca/floof/`
19. `https://api.openbrewerydb.org/breweries`
20. `https://ghibliapi.herokuapp.com/films`
21. `https://poetrydb.org/random`
22. `https://www.thecocktaildb.com/api/json/v1/1/random.php`
23. `https://www.themealdb.com/api/json/v1/1/random.php`
24. `https://pokeapi.co/api/v2/pokemon/pikachu`
25. `https://xkcd.com/info.0.json`
26. `https://swapi.dev/api/people/1`
27. `https://api.coingecko.com/api/v3/exchange_rates`
28. `https://www.reddit.com/r/popular.json`
29. `https://en.wikipedia.org/api/rest_v1/page/summary/Stack_Overflow`
30. `https://jsonplaceholder.typicode.com/todos/1`

## Usage

Run the scraper:

```bash
python unified_scraper.py
```

## Requirements

- Python 3 with `requests`

No API keys are necessary to access the listed endpoints.
