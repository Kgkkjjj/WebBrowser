package main

import (
	"io"
	"log"
	"net/http"
)

func fetchHandler(w http.ResponseWriter, r *http.Request) {
	url := r.URL.Query().Get("url")
	if url == "" {
		http.Error(w, "missing url", http.StatusBadRequest)
		return
	}
	resp, err := http.Get(url)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	defer resp.Body.Close()
	for k, v := range resp.Header {
		for _, vv := range v {
			w.Header().Add(k, vv)
		}
	}
	w.WriteHeader(resp.StatusCode)
	io.Copy(w, resp.Body)
}

func main() {
	http.HandleFunc("/fetch", fetchHandler)
	log.Println("Go helper listening on :8090")
	log.Fatal(http.ListenAndServe(":8090", nil))
}
