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
