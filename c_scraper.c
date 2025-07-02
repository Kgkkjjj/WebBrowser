#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct string {
    char *ptr;
    size_t len;
};

static void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(1);
    s->ptr[0] = '\0';
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

struct api { const char *name; const char *url; };

static struct api apis[] = {
    {"Public APIs", "https://api.publicapis.org/entries"},
    {"Random Dog", "https://dog.ceo/api/breeds/image/random"},
    {"Cat Fact", "https://catfact.ninja/fact"},
    {"Agify", "https://api.agify.io?name=John"},
    {"Genderize", "https://api.genderize.io?name=John"},
    {"Nationalize", "https://api.nationalize.io?name=John"},
    {"Joke", "https://official-joke-api.appspot.com/jokes/random"},
    {"Bored", "https://www.boredapi.com/api/activity"},
    {"Bitcoin Price", "https://api.coindesk.com/v1/bpi/currentprice.json"},
    {"IPify", "https://api.ipify.org?format=json"},
};

int main(void) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl init failed\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(apis)/sizeof(apis[0]); ++i) {
        struct string s; init_string(&s);
        curl_easy_setopt(curl, CURLOPT_URL, apis[i].url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            printf("--- %s ---\n%s\n", apis[i].name, s.ptr);
        } else {
            fprintf(stderr, "Error fetching %s: %s\n", apis[i].name, curl_easy_strerror(res));
        }
        free(s.ptr);
    }
    curl_easy_cleanup(curl);
    return 0;
}
