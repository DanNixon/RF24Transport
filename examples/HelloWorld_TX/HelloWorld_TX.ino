/**
 * RF24Transport basic example sending node
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

const uint16_t this_node = 1;

const uint16_t other_node = 0;

unsigned long last_sent;
unsigned long packets_sent;

void setup(void)
{
  Serial.begin(115200);
 
  SPI.begin();
  radio.begin();
  network.begin(90, this_node);
}

void loop(void)
{
  transport.update();

  if((millis() - last_sent) > 1000)
  {
    last_sent = millis();

    Serial.println("Sending...");

    char msg[100] = "hello world, test text, this is a lot more test, such text, very long, wow";

    bool ok = transport.write(other_node, &msg, sizeof(msg));

    if(ok)
      Serial.println("ok.");
    else
      Serial.println("failed.");
  }

  while(transport.available())
  {
    uint32_t their_time;
    uint16_t their_address;

    transport.read(&their_address, &their_time, sizeof(uint32_t));

    Serial.print("Got time from ");
    Serial.print(their_address);
    Serial.print(": ");
    Serial.println(their_time);
  }
}
