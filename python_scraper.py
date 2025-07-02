import requests

APIS = {
    "Public APIs": "https://api.publicapis.org/entries",
    "Random Dog": "https://dog.ceo/api/breeds/image/random",
    "Cat Fact": "https://catfact.ninja/fact",
    "Agify": "https://api.agify.io?name=John",
    "Genderize": "https://api.genderize.io?name=John",
    "Nationalize": "https://api.nationalize.io?name=John",
    "Joke": "https://official-joke-api.appspot.com/jokes/random",
    "Bored": "https://www.boredapi.com/api/activity",
    "Bitcoin Price": "https://api.coindesk.com/v1/bpi/currentprice.json",
    "IPify": "https://api.ipify.org?format=json",
    "Random User": "https://randomuser.me/api/",
    "Advice": "https://api.adviceslip.com/advice",
    "Kanye Quote": "https://api.kanye.rest/",
    "YesNo": "https://yesno.wtf/api",
    "Chuck Norris": "https://api.chucknorris.io/jokes/random",
    "Ron Swanson": "https://ron-swanson-quotes.herokuapp.com/v2/quotes",
    "ISS Now": "http://api.open-notify.org/iss-now.json",
    "Random Fox": "https://randomfox.ca/floof/",
    "Open Brewery": "https://api.openbrewerydb.org/breweries",
    "Studio Ghibli": "https://ghibliapi.herokuapp.com/films",
    "Random Poem": "https://poetrydb.org/random",
    "Cocktail": "https://www.thecocktaildb.com/api/json/v1/1/random.php",
    "Meal": "https://www.themealdb.com/api/json/v1/1/random.php",
    "Pikachu": "https://pokeapi.co/api/v2/pokemon/pikachu",
    "xkcd": "https://xkcd.com/info.0.json",
    "Star Wars": "https://swapi.dev/api/people/1",
    "CoinGecko": "https://api.coingecko.com/api/v3/exchange_rates",
    "Reddit": "https://www.reddit.com/r/popular.json",
    "Wikipedia": "https://en.wikipedia.org/api/rest_v1/page/summary/Stack_Overflow",
    "JSONPlaceholder": "https://jsonplaceholder.typicode.com/todos/1",
}


def main():
    for name, url in APIS.items():
        try:
            resp = requests.get(url, timeout=10)
            resp.raise_for_status()
            print(f"--- {name} ---")
            print(resp.json())
        except Exception as e:
            print(f"Error fetching {name}: {e}")


if __name__ == "__main__":
    main()
