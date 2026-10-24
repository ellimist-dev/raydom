// Serves a web build for local testing: static files, never cached, with each request logged. / is
// the showcase.
package main

import (
	"flag"
	"log"
	"net/http"
	"time"
)

func main() {
	dir := flag.String("dir", "build-web/examples", "directory to serve")
	// 127.0.0.1 rather than localhost: browsers may resolve localhost to ::1 first, where another server
	// (Docker Desktop takes *:8080) can answer instead.
	addr := flag.String("addr", "127.0.0.1:8090", "address to listen on")
	flag.Parse()

	files := http.FileServer(http.Dir(*dir))
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		w.Header().Set("Cache-Control", "no-store")
		if r.URL.Path == "/" {
			r.URL.Path = "/showcase.html"
		}
		files.ServeHTTP(w, r)
		log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(start).Round(time.Microsecond))
	})

	log.Printf("serving %s at http://%s", *dir, *addr)
	log.Fatal(http.ListenAndServe(*addr, handler))
}
