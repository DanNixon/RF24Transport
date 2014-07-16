/**
 * RF24 wirelesss node transport layer
 *
 * RF24Transport
 * Dan Nixon, 2014
 * dan-nixon.com
 */

#ifndef RF24_TRANSPORT_H
#define RF24_TRANSPORT_H

#include <Arduino.h>
#include <inttypes.h>
#include <RF24Network.h>

#define MAX_TRANSPORT_PACKET_PAYLOAD_SIZE   19
#define NUM_TRANSPORT_BUFFERS               16

#define TRANSPORT_TIMEOUT 5000

typedef uint8_t pktid_t;

/**
 * Type of transport later messages
 */
enum TransportPayloadType
{
  PACKET_HEAD,
  PACKET_TAIL,
  PACKET_DATA
};

/**
 * Payload sent in a network paylaod to transmit a segemnt of a transport
 * paylaod
 */
struct TransportPayload
{
  pktid_t packet_id;
  uint16_t seq;
  TransportPayloadType type;
  uint8_t payload[MAX_TRANSPORT_PACKET_PAYLOAD_SIZE];
};

/**
 * Payload sent with a transport header packet
 */
struct TransportPacketHeader
{
  uint32_t length;
};

/**
 * Type to hold a transport level payload in a buffer while it is being received
 * and before it is collected by a call to read()
 */
struct TransportReceiveBuffer
{
  pktid_t packet_id;
  uint16_t from_addr;
  bool complete;

  void *payload;
  uint32_t payload_len;

  uint32_t head_arrival_time;
};

class RF24Transport
{
  public:
    RF24Transport(RF24Network &network);
    ~RF24Transport();

    void update();
    bool available();
    bool write(uint16_t to_addr, const void *payload, uint32_t len);
    void read(uint16_t *from_addr, void *payload, uint32_t max_len);

    uint8_t num_free_buffers();

  private:
    uint8_t clean_buffers(uint32_t timeout);

    pktid_t get_new_packet_id();

    TransportReceiveBuffer *create_rx_buffer(pktid_t packet_id, uint32_t length, uint16_t from_addr);
    TransportReceiveBuffer *get_rx_buffer(pktid_t packet_id);
    void delete_rx_buffer(TransportReceiveBuffer *buffer);

    RF24Network *network;

    pktid_t next_packet_id;

    TransportReceiveBuffer *buffers[NUM_TRANSPORT_BUFFERS];
};

#endif
