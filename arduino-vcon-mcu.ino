// Copyright (c) 2024 Cesanta Software Limited
//
// This is a sample firmware for the Master MCU, see https://vcon.io
// It exchanges data over Serial: MASTER_MCU  <-- Serial --> VCON <-- TCP
// This sketch communicates 2-way with the VCON module via JSON-RPC protocol.
//
// In this example, Master acts as both RPC client and server:
//   - as a server, it gets "sys.time", "vcon.event", "net.recv" notifications
//   - as a client, it call "net.send" RPC on VCON as a response to "net.recv"
//
// When VCON receives data via TCP, WS, or MQTT connection, it forwards that
// data to the Serial (to this sketch), and depending on RPC/transparent mode,
// - In the transparent mode ($.host.mode=2), VCON writes that data as-is
// - In the RPC mode ($.host.mode=1), VCON sends us "net.recv" RPC notification
//
// Thus, a transparent mode is simpler to handle, but we (Master MCU) cannot
// talk to the VCON module. The RPC mode is a bit harder to process, cause it
// requires JSON parsing/printing, but allows us to talk to both VCON and a
// remote peer. This sketch assumes VCON is in RPC mode, and it uses "str.h"
// single-header library which makes JSON easy to handle.
//
// How to test:
//   - On VCON module, set the following configuration parameters:
//      $.host.mode=1        VCON/Master communication into RPC mode
//      $.tcp.port=1234      Open listening TCP port 1234
//      $.uart.delim=1       Delimits VCON/Master UART datagrams with newline
//   - Open Arduino serial monitor
//   - Connect to VCON TCP port: "nc VCON_IP_ADDRESS 1234", type data

#include "str.h"  // https://github.com/cesanta/str

// These are RPC frames that VCON can send us:
// {"method": "net.recv", "params": {"base64": "BASE64_ENCODED_DATA"}}
// {"method": "sys.time", "params": EPOCH_IN_SECONDS}
// {"method": "vcon.status", "params": "CURRENT_NETWORK_STATE"}
//
// It is very easy to test this Arduino sketch without VCON module attached:
// just open a serial console, and enter various JSON-RPC strings like this:
// {"method":"net.recv","params":{"base64":"aGVsbG8K"}}
static void process_rpc_frame(char *buf, size_t len) {
  int n = 0, o = json_get(buf, len, "$.method", &n);         // Fetch method
  if (n == 10 && memcmp(&buf[o + 1], "net.recv", 8) == 0) {  // Is it net.recv?
    // Locate data and base64-decode it in-place
    if ((o = json_get(buf, len, "$.params.base64", &n)) > 0 &&
        (n = xb64_decode(&buf[o + 1], n - 2, &buf[o], n)) > 0) {
      // Tell VCON to respond to the remote host: print "sent.send" request
      // net.send accepts "data" (plain text), or "base64", or "hex" param
      // Note: we send a newline delimiter at the end of the JSON string
      xprintf([](char ch, void *) { Serial.print(ch); }, NULL,
              "{\"method\":\"net.send\",%m:{%m:\"Got data: %M\"}}\n",
              XESC("params"),  // The right way to escape and double-quote
              XESC("data"),    // a NUL-terminated string
              fmt_esc, n, &buf[o]);
    }
  }
}

static void read_byte(unsigned char ch) {
  static unsigned char buf[256];  // Holds received JSON-RPC request or response
  static size_t len;
  if (ch == '\n') {
    process_rpc_frame(buf, len);  // Received RPC frame, process it
    len = 0;                      // Reset receive buffer
  } else {
    buf[len++] = ch;                  // Buffer received character
    if (len >= sizeof(buf)) len = 0;  // On overflow, silently reset
  }
}

void setup(void) {
  Serial.begin(115200);        // Initialise Serial
  while (!Serial) delay(100);  // Wait until Serial is ready
}

void loop(void) {
  if (Serial.available()) read_byte(Serial.read());
}
