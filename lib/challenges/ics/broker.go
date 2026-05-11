package main

import (
	"crypto/tls"
	"log"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync/atomic"
	"syscall"
	"time"

	mqtt "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
	"github.com/mochi-mqtt/server/v2/packets"
)

type brokerState int32

const (
	stateInit        brokerState = iota // waiting for badge to subscribe to "drm"
	stateWaitFor67                      // sent 77, waiting for badge to reply "67"
	stateWaitFor10                      // sent 42, waiting for badge to reply "10"
	stateWaitProduct                    // sent 6+13, waiting for badge to publish product
	stateWaitValve                      // waiting for "open_valve" on "water"
	stateDone                           // challenge complete
)

type ICSHook struct {
	mqtt.HookBase
	server *mqtt.Server
	state  atomic.Int32
}

func (h *ICSHook) ID() string { return "ics-hook" }

func (h *ICSHook) Provides(b byte) bool {
	return b == mqtt.OnPublished || b == mqtt.OnSubscribed
}

func (h *ICSHook) pub(topic, payload string) {
	if err := h.server.Publish(topic, []byte(payload), false, 0); err != nil {
		log.Printf("[broker] publish %q → %q failed: %v", payload, topic, err)
	}
}

func (h *ICSHook) OnSubscribed(cl *mqtt.Client, pk packets.Packet, reasonCodes []byte) {
	for _, sub := range pk.Filters {
		if sub.Filter == "drm" {
			if h.state.CompareAndSwap(int32(stateInit), int32(stateWaitFor67)) {
				log.Printf("[broker] %s subscribed to 'drm' — sending 77", cl.ID)
				go func() {
					time.Sleep(200 * time.Millisecond)
					h.pub("drm", "77")
				}()
			}
			return
		}
	}
}

func (h *ICSHook) OnPublished(cl *mqtt.Client, pk packets.Packet) {
	if cl == nil || !strings.HasPrefix(cl.ID, "NSEC-") {
		return
	}

	payload := strings.TrimSpace(string(pk.Payload))

	switch pk.TopicName {
	case "drm":
		h.onDrm(payload)
	case "water":
		h.onWater(payload)
	}
}

func (h *ICSHook) onDrm(payload string) {
	switch brokerState(h.state.Load()) {
	case stateWaitFor67:
		if payload == "67" {
			log.Println("[broker] S1→S2: received '67', sending 42")
			h.state.Store(int32(stateWaitFor10))
			h.pub("drm", "42")
		}

	case stateWaitFor10:
		if payload == "10" {
			log.Println("[broker] S2→S3: received '10', sending 6 (even) then 13")
			h.state.Store(int32(stateWaitProduct))
			go func() {
				h.pub("drm", "6")
				time.Sleep(100 * time.Millisecond)
				h.pub("drm", "13")
			}()
		}

	case stateWaitProduct:
		n, err := strconv.ParseInt(payload, 10, 64)
		if err == nil {
			log.Printf("[broker] S4→S5: received product %d, waiting for open_valve", n)
			h.state.Store(int32(stateWaitValve))
			h.pub("zigbee2mqtt/0xbc33acfffed22bf2/set", `{"state": "TOGGLE"}`)
		}
	}
}

func (h *ICSHook) onWater(payload string) {
	if brokerState(h.state.Load()) == stateWaitValve && payload == "open_valve" {
		h.state.Store(int32(stateDone))
		log.Println("[broker] Challenge complete: 'open_valve' received on 'water'")
	}
}

func main() {
	server := mqtt.New(nil)
	_ = server.AddHook(new(auth.AllowHook), nil)

	hook := &ICSHook{server: server}
	_ = server.AddHook(hook, nil)

	cert, err := tls.LoadX509KeyPair("server.crt", "server.key")
	if err != nil {
		log.Fatalf("Error loading certificates: %v", err)
	}

	tcp := listeners.NewTCP(listeners.Config{
		ID:      "t1",
		Address: ":8883",
		TLSConfig: &tls.Config{
			Certificates: []tls.Certificate{cert},
		},
	})
	if err = server.AddListener(tcp); err != nil {
		log.Fatal(err)
	}

	go func() {
		if err := server.Serve(); err != nil {
			log.Fatal(err)
		}
	}()
	log.Println("MQTT Broker running on :8883")

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)
	<-sigs

	log.Println("Shutting down...")
	server.Close()
}
