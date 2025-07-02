package main

import (
    "encoding/json"
    "fmt"
    "io/ioutil"
    "net/http"
    "time"
)

var apis = map[string]string{
    "Public APIs":    "https://api.publicapis.org/entries",
    "Random Dog":     "https://dog.ceo/api/breeds/image/random",
    "Cat Fact":       "https://catfact.ninja/fact",
    "Agify":          "https://api.agify.io?name=John",
    "Genderize":      "https://api.genderize.io?name=John",
    "Nationalize":    "https://api.nationalize.io?name=John",
    "Joke":           "https://official-joke-api.appspot.com/jokes/random",
    "Bored":          "https://www.boredapi.com/api/activity",
    "Bitcoin Price":  "https://api.coindesk.com/v1/bpi/currentprice.json",
    "IPify":          "https://api.ipify.org?format=json",
}

func main() {
    client := &http.Client{Timeout: 10 * time.Second}
    for name, url := range apis {
        resp, err := client.Get(url)
        if err != nil {
            fmt.Printf("Error fetching %s: %v\n", name, err)
            continue
        }
        body, err := ioutil.ReadAll(resp.Body)
        resp.Body.Close()
        if err != nil {
            fmt.Printf("Error reading %s: %v\n", name, err)
            continue
        }
        fmt.Printf("--- %s ---\n", name)
        var out interface{}
        if json.Unmarshal(body, &out) == nil {
            fmt.Printf("%v\n", out)
        } else {
            fmt.Printf("%s\n", body)
        }
    }
}
