/**
 * RF24Transport basic example receiving node
 *
 * RF24Transport
 * Dan Nixon, 2014
 * dan-nixon.com
 */

#include <RF24Transport.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio(9,10);

RF24Network network(radio);

RF24Transport transport(network);

const uint16_t this_node = 0;

const uint16_t other_node = 1;

void setup(void)
{
  Serial.begin(115200);
 
  Serial.println(sizeof(TransportPayload));

  SPI.begin();
  radio.begin();
  network.begin(90, this_node);
}

void loop(void)
{
  transport.update();

  while(transport.available())
  {
    char msg[100];
    uint16_t from_addr;
    transport.read(&from_addr, &msg, sizeof(msg));

    Serial.print("Node ");
    Serial.print(from_addr);
    Serial.print(" says \"");
    Serial.print(msg);
    Serial.println("\"");

    uint32_t time = millis();

    bool ok = transport.write(from_addr, &time, sizeof(uint32_t));

    if(!ok)
      Serial.println("Could not send response");
  }
}
