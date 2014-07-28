#include "RF24Transport.h"

/**
 * \brief Construct a new transport layer.
 *
 * \param network Reference to the network layer to use
 */
RF24Transport::RF24Transport(RF24Network &network)
{
  this->network = &network;

  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    this->buffers[i] = NULL;
  }
}

/**
 * \brief Destructor.
 *
 * Cleans up memory allocated by remaining buffers
 */
RF24Transport::~RF24Transport()
{
  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    delete this->buffers[i]->payload;
    delete this->buffers[i];
  }
}

/**
 * \brief Reads any new messages from the network layer and processes them.
 */
void RF24Transport::update()
{
  /*
   * Make sure we have the most free space possible for new data
   */
  this->clean_buffers(TRANSPORT_TIMEOUT);

  /*
   * Get new packets from network layer
   */
  this->network->update();

  while(this->network->available())
  {
    RF24NetworkHeader header;
    TransportPayload pkt;
    this->network->read(header, &pkt, sizeof(TransportPayload));

    /*
     * Process the packet
     */
    switch(pkt.type)
    {
      case PACKET_HEAD:
      {
        TransportPacketHeader *hdr = (TransportPacketHeader *) pkt.payload;
        TransportReceiveBuffer *buffer = this->create_rx_buffer(
            pkt.packet_id, hdr->length, header.from_node);
        break;
      }
      case PACKET_DATA:
      {
        TransportReceiveBuffer *buffer = this->get_rx_buffer(pkt.packet_id);
        uint32_t offset = pkt.seq * MAX_TRANSPORT_PACKET_PAYLOAD_SIZE;
        memcpy(((buffer->payload)+offset), pkt.payload, sizeof(pkt.payload));
        break;
      }
      case PACKET_TAIL:
      {
        TransportReceiveBuffer *buffer = this->get_rx_buffer(pkt.packet_id);

        buffer->complete = true;
        break;
      }
    }
  }
}

/**
 * \brief Checks to see if there is a complete transport payload available for reading.
 *
 * \return True if a payload can be read, false otherwise
 */
bool RF24Transport::available()
{
  TransportReceiveBuffer *buffer = NULL;

  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    buffer = this->buffers[i];

    if(buffer && buffer->complete)
      return true;
  }

  return false;
}

/**
 * \brief Send a payload to a node.
 *
 * \param to_addr Node address to transmit to
 * \param payload Pointer to payload to send
 * \param len Length of payload in bytes
 *
 * \return True if full payload was sent, false otherwise
 */
bool RF24Transport::write(uint16_t to_addr, const void *payload, uint32_t len)
{
  int32_t length_remaining = len;
  TransportPayload pkt;

  /*
   * Set packet details for header
   */
  pkt.packet_id = this->next_packet_id;
  pkt.seq = 0;
  pkt.type = PACKET_HEAD;

  /*
   * Generate header packet payload
   */
  TransportPacketHeader head = {len};
  memcpy(pkt.payload, &head, sizeof(head));

  /*
   * Generate network header
   * Same header used for all transmissions of a single transport payload
   */
  RF24NetworkHeader header(to_addr);

  /*
   * Attempt to send the header packet
   * Report failure if no ack.
   */
  if(!this->network->write(header, &pkt, sizeof(pkt)))
  {
    return false;
  }

  /*
   * Will be sending data packets for a while now
   */
  pkt.type = PACKET_DATA;
  while(length_remaining > 0)
  {
    /*
     * Find the byte in the transport payload from which the next network
     * payload will transmit
     */
    uint32_t offset = pkt.seq * MAX_TRANSPORT_PACKET_PAYLOAD_SIZE;

    /*
     * Calculate how much data needs to be copied to the payload
     * For all bu the last data packet this is MAX_TRANSPORT_PACKET_PAYLOAD_SIZE
     */
    uint32_t msg_len = len - offset;
    if((msg_len - 1) > MAX_TRANSPORT_PACKET_PAYLOAD_SIZE)
      msg_len = MAX_TRANSPORT_PACKET_PAYLOAD_SIZE;

    /*
     * Copy data from transport payload to network payload
     */
    memcpy(pkt.payload, (((uint8_t*)payload)+offset), msg_len);

    /*
     * Attempt to send the message
     * Report failure if no ack.
     */
    if(!this->network->write(header, &pkt, sizeof(pkt)))
    {
      return false;
    }

    /*
     * Calculate how much of the transport payload is left to send
     * Increment packet sequence number
     */
    length_remaining -= MAX_TRANSPORT_PACKET_PAYLOAD_SIZE;
    pkt.seq++;
  }

  /*
   * Send tail packet
   * Indicates end of data only
   * Payload and sequence number are ignored
   */
  pkt.type = PACKET_TAIL;
  if(!this->network->write(header, &pkt, sizeof(pkt)))
  {
    return false;
  }

  /*
   * Give next packet a new ID
   */
  this->next_packet_id++;

  /*
   * If we got this far it is a safe assumption the other node got the full
   * transport payload
   */
  return true;
}

/**
 * \brief Read a transport payload from the buffer.
 *
 * \param from_addr Pointer to an uint16_t to store the transmitting node address
 * \param payload Pointer to memory to store the payload in
 * \param max_len Maximum number of bytes to copy into payload
 */
void RF24Transport::read(uint16_t *from_addr, void *payload, uint32_t max_len)
{
  TransportReceiveBuffer *buffer = NULL;

  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    buffer = this->buffers[i];

    if(buffer && buffer->complete)
      break;
  }

  if(buffer == NULL)
    return;

  if(max_len > buffer->payload_len)
    max_len = buffer->payload_len;

  *from_addr = buffer->from_addr;
  memcpy(payload, buffer->payload, max_len);

  delete_rx_buffer(buffer);
}

/**
 * \brief Creates a new receiver buffer, adds it to the linked list and returns a pointer to it.
 *
 * \param packet_id The ID of the packet
 * \param length Length of the transport payload
 * \param from_addr Address of the transmitting node
 */
TransportReceiveBuffer *RF24Transport::create_rx_buffer(pktid_t packet_id, uint32_t length, uint16_t from_addr)
{
  TransportReceiveBuffer *new_buffer = new TransportReceiveBuffer();

  new_buffer->packet_id = packet_id;
  new_buffer->payload_len = length;
  new_buffer->from_addr = from_addr;
  new_buffer->complete = false;
  new_buffer->head_arrival_time = millis();

  new_buffer->payload = malloc(length);

  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    if(this->buffers[i] == NULL)
    {
      this->buffers[i] = new_buffer;
      return this->buffers[i];
    }
  }

}

/**
 * \brief Gets a pointer to a receive buffer based on it's packet number.
 *
 * \param packet_id Packet ID to get
 */
TransportReceiveBuffer *RF24Transport::get_rx_buffer(pktid_t packet_id)
{
  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    if(this->buffers[i])
    {
      if(this->buffers[i]->packet_id == packet_id)
      {
        return this->buffers[i];
      }
    }
  }

  return NULL;
}

/**
 * \brief Removes a buffer from the linked list.
 *
 * \param buffer The buffer to remove
 */
void RF24Transport::delete_rx_buffer(TransportReceiveBuffer *buffer)
{
  /*
   * In any case remove the payload
   */
  delete buffer->payload;

  uint8_t i;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    if(this->buffers[i] == buffer)
    {
      delete this->buffers[i];
      this->buffers[i] = NULL;
    }
  }
}

/**
 * \brief Gets the number of free buffer positions
 *
 * \return Number of free buffers
 */
uint8_t RF24Transport::num_free_buffers()
{
  uint8_t i, free;

  free = 0;
  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    if(this->buffers[i] == NULL)
      free++;
  }

  return free;
}

/**
 * \brief Deletes any incomplete buffers older than a given timeout.
 *
 * \param timeout Any incomplete buffers older than this will be deleted
 */
uint8_t RF24Transport::clean_buffers(uint32_t timeout)
{
  uint8_t i, cleaned;
  cleaned = 0;

  for(i = 0; i < NUM_TRANSPORT_BUFFERS; i++)
  {
    if(this->buffers[i])
    {
      uint8_t old_packet = (millis() - this->buffers[i]->head_arrival_time) > timeout;
      if(!this->buffers[i]->complete && old_packet)
      {
        this->delete_rx_buffer(this->buffers[i]);
      }
    }
  }

  return cleaned;
}
