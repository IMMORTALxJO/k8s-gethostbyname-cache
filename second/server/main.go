package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"net"
	"os"
	"strings"
	"sync"
	"syscall"
	"time"
)

const socketPath = "/dns_cache/dns.sock"

func main() {
	if err := os.RemoveAll(socketPath); err != nil {
		fmt.Println("Socket deletion error:", err)
		return
	}

	syscall.Umask(0077)
	l, err := net.Listen("unix", socketPath)
	if err != nil {
		fmt.Println("Socket creation error:", err)
		return
	}
	if err := os.Chmod("/client/dns.sock", 0700); err != nil {
		fmt.Println("Socket creation error:", err)
	}

	defer l.Close()

	fmt.Println("Listening socket:", socketPath)

	for {
		conn, err := l.Accept()
		if err != nil {
			fmt.Println("Connection error:", err)
			continue
		}
		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {
	defer conn.Close()

	host, err := bufio.NewReader(conn).ReadString('\n')
	if err != nil {
		fmt.Println("Read error:", err)
		return
	}
	host = strings.TrimSpace(host)
	fmt.Printf("Got message: '%s'\n", host)

	response := ResolveIP(host)
	if _, err := conn.Write([]byte(response)); err != nil {
		fmt.Println("Error during send:", err)
		return
	}
}

type cacheItem struct {
	IPs        []net.IP
	Expiration time.Time
}

var ipCache = make(map[string]cacheItem)
var cacheLock sync.RWMutex

const ttl = 30 * time.Second

func ResolveIP(host string) string {
	cacheLock.RLock()
	if item, found := ipCache[host]; found && time.Now().Before(item.Expiration) {
		cacheLock.RUnlock()
		fmt.Println("Cache hit", host)
		return getRandomIP(item.IPs).String()
	}
	cacheLock.RUnlock()
	fmt.Println("Cache warmup", host)

	ips, err := net.LookupIP(host)
	if err != nil {
		fmt.Printf("Resolve %s: %v\n", host, err)
		return ""
	}

	cacheLock.Lock()
	ipCache[host] = cacheItem{
		IPs:        ips,
		Expiration: time.Now().Add(ttl),
	}
	cacheLock.Unlock()
	return getRandomIP(ips).String()
}

func getRandomIP(ips []net.IP) net.IP {
	if len(ips) == 0 {
		return nil
	}
	index := rand.Intn(len(ips))
	return ips[index]
}
