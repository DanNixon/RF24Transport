/**
 * RF24Transport Serial console demo
 *
 * RF24Transport
 * Dan Nixon, 2014
 * dan-nixon.com
 */

#include <RF24Transport.h>
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

//Comment out this line for machine-machine communication
#define VERBOSE_SERIAL

#define MAX_MESSAGE_LEN 256

RF24 radio(9,10);
RF24Network network(radio);
RF24Transport transport(network);

uint16_t this_node = 0;
uint16_t other_node = 0;

struct message_t
{
  uint32_t time;
  char message[MAX_MESSAGE_LEN];
};

message_t *msg_to_send = NULL;
uint16_t msg_to_send_text_pos = 0;

void setup(void)
{
  Serial.begin(115200);
 
  //Set the node addresses
  pinMode(7, INPUT);
  digitalWrite(7, HIGH);
  delay(100);

  if(!digitalRead(7))
    this_node = 1;
  else
    other_node = 1;

#ifdef VERBOSE_SERIAL
  //Tell the user who is who
  Serial.print("My address: ");
  Serial.println(this_node);
  Serial.print("Their address: ");
  Serial.println(other_node);
#endif

  SPI.begin();
  radio.begin();
  network.begin(90, this_node);
}

void loop(void)
{
  transport.update();

  //If we have serial data
  while(Serial.available())
  {
    char c = Serial.read();

    if(c == 13 || c == 10)
    {
      send_message();
    }
    else
    {
      if(msg_to_send == NULL)
      {
        msg_to_send = new message_t();
        msg_to_send_text_pos = 0;
      }

      msg_to_send->message[msg_to_send_text_pos] = c;

      msg_to_send_text_pos++;

      if(msg_to_send_text_pos == (MAX_MESSAGE_LEN - 1))
        send_message();
    }
  }

  //If we have RF data
  while(transport.available())
  {
    message_t msg;
    uint16_t their_address;

    transport.read(&their_address, &msg, sizeof(message_t));

#ifdef VERBOSE_SERIAL
    Serial.print(msg.time);
    Serial.print(" [");
    Serial.print(their_address);
    Serial.print("]: ");
#endif
    Serial.println(msg.message);
  }
}

void send_message()
{
  msg_to_send->message[msg_to_send_text_pos] = '\0';

  msg_to_send->time = millis();
  
  bool ok = transport.write(other_node, msg_to_send, sizeof(message_t));

  if(!ok)
  {
#ifdef VERBOSE_SERIAL
    Serial.println("Failed to send message!");
#else
    Serial.println("SEND_ERROR");
#endif
  }

  delete msg_to_send;

  msg_to_send_text_pos = 0;
  msg_to_send = new message_t();
}
