
#include <MeAuriga.h>
#include <SoftwareSerial.h>
#include "cmd_process.h"

// motors
MeEncoderOnBoard MOTOR_LEFT(SLOT1);
MeEncoderOnBoard MOTOR_RIGT(SLOT2);
MeEncoderOnBoard *MOTORS[2] = {&MOTOR_LEFT, &MOTOR_RIGT};

MeUltrasonicSensor ultraSensor(PORT_6); // ultrasonic
MeRGBLed led_ring(0, 12);

MeBluetooth bte(PORT_5); // external btle

CommandBuffer ser(&Serial, 0, 10, 10);
CommandBuffer ble(&bte, 0, 0, 10);
CommandBuffer *bufs[2] = { &ser, &ble };

void isr_process_encoder(uint8_t ind) {
	MeEncoderOnBoard *motor = MOTORS[ind];
	if (digitalRead(motor->getPortB()) == 0) {
		motor->pulsePosMinus();
	} else {
		motor->pulsePosPlus();
	}
}
void isr_process_encoder_left(void) { isr_process_encoder(0); }
void isr_process_encoder_rigt(void) { isr_process_encoder(1); }

void motor_done(int16_t motor, int16_t ext) {
	// report back to the initiator that the motor has finished moving
	CommandBuffer *out = ext == 0 ? &ser : &ble;
	out->print("!MotorDone,");
	switch(motor) {
		case 1: out->print("left\n"); break;
		case 2: out->print("rigt\n"); break;
		default: out->print("%d\n", motor);
	}
}


void setup() {
	attachInterrupt(MOTOR_LEFT.getIntNum(), isr_process_encoder_left, RISING);
	attachInterrupt(MOTOR_RIGT.getIntNum(), isr_process_encoder_rigt, RISING);
	Serial.begin(115200);
	bte.begin(115200);
	led_ring.setpin(44);

	// Set PWM 8KHz
	TCCR1A = _BV(WGM10);
	TCCR1B = _BV(CS11) | _BV(WGM12);

	TCCR2A = _BV(WGM21) | _BV(WGM20);
	TCCR2B = _BV(CS21);

	MOTOR_LEFT.setPulse(9);
	MOTOR_RIGT.setPulse(9);
	MOTOR_LEFT.setRatio(39.267);
	MOTOR_RIGT.setRatio(39.267);
	MOTOR_LEFT.setPosPid(1.8,0,1.2);
	MOTOR_RIGT.setPosPid(1.8,0,1.2);
	MOTOR_LEFT.setSpeedPid(0.18,0,0);
	MOTOR_RIGT.setSpeedPid(0.18,0,0);

	led_ring.setColor(10,10,10);
    led_ring.show();
    delay(100);
    led_ring.setColor(0,0,0);
    led_ring.show();
    delay(100);
    led_ring.setColor(10,10,10);
    led_ring.show();
    delay(100);
    led_ring.setColor(0,0,0);
    led_ring.show();
}

// cold to clear LED status lights using the ring above
unsigned long clear_ring_at[13] = {0};
void set_ring_led(uint8_t i, unsigned long length, uint8_t r, uint8_t g, uint8_t b) {
	led_ring.setColor(i, r, g, b);
	clear_ring_at[i] = millis() + length;
}

void loop() {
	process_commander(&ser); // process commands over serial
	process_commander(&ble); // process commands over bluetooth

	// update motors as necessary
	MOTOR_LEFT.loop();
	MOTOR_RIGT.loop();

	if(lspeed > 0 && rspeed > 0) { // log any movement
		for(int i = 0; i < sizeof(bufs)/sizeof(bufs[0]); i++) {
			bufs[i]->print("Speeds=(%f,%f)  Positions=(%ld,%ld)\n",
				MOTOR_LEFT.getCurrentSpeed(), MOTOR_RIGT.getCurrentSpeed(),
				MOTOR_LEFT.getCurPos(), MOTOR_RIGT.getCurPos()
			);
		}
	}

	// clear applicable LED lights
	for(size_t i = 0; i < 12; ++i) {
		if(clear_ring_at[i] != 0 && clear_ring_at[i] < millis()) {
			clear_ring_at[i] = 0;
			led_ring.setColor(i, 0, 0, 0);
			led_ring.show();
		}
	}
}

// read in and process commands over a serial channel (USB, BLE)
void process_commander(CommandBuffer *cmdbuf) {
	cmdbuf->read();
	size_t cmds_len;
	char** cmds = cmdbuf->parse(&cmds_len);
	process_cmd(cmdbuf, cmds, cmds_len);
}

// process any input commands
void process_cmd(CommandBuffer *cmdbuf, char** cmds, size_t cmds_len) {
	if(cmds && cmds_len > 0) {
		// for debug - flash an LED if a message came in, according to which channel
		if(cmdbuf == &ser) {
			set_ring_led(1, 500, 10, 0, 0);
		} else if(cmdbuf == &ble) {
			set_ring_led(2, 500, 0, 0, 10);
		}

		// cmdbuf->printf("!DBG,Commands,%d,", cmds_len);
		// for(size_t i = 0; i < cmds_len; i++) {
		// 	cmdbuf->printf("%s,", cmds[i]);
		// }
		// cmdbuf->printf("\n");

		// Move,left_deg,right_deg[,left_speed,right_speed]
		// Stop
		// QueryDistanceCM
		if(strcmp(cmds[0], "Move") == 0) {
			if(cmds_len == 3 || cmds_len == 5) {
				long left = strtol(cmds[1], nullptr, 10);
				long rigt = strtol(cmds[2], nullptr, 10);
				float lefts = 100.0F;
				float rigts = 100.0F;

				if(cmds_len == 5) {
					lefts = (float) strtod(cmds[3], nullptr);
					rigts = (float) strtod(cmds[4], nullptr);
				}

				cmdbuf->print("Requested move: (%ld, %ld) with speed (%f, %f)\n", left, rigt, lefts, rigts);

				int16_t ext = cmdbuf == &ser ? 0 : 1;
				MOTOR_LEFT.move(-left, lefts, ext, motor_done);
				MOTOR_RIGT.move( rigt, rigts, ext, motor_done);

				cmdbuf->print("!OK,Move,%ld,%ld", left, rigt);
				if(cmds_len == 5) cmdbuf->print(",%f,%f", lefts, rigts);
				cmdbuf->print("\n");
			}
		} else if(cmds_len == 1 && strcmp(cmds[0], "Stop") == 0) {
			MOTOR_LEFT.move(0);
			MOTOR_RIGT.move(0);
			cmdbuf->print("!OK,Stop\n");
		} else if(cmds_len == 1 && strcmp(cmds[0], "ResetPos") == 0) {
			MOTOR_LEFT.setPulsePos(0);
			MOTOR_RIGT.setPulsePos(0);
			cmdbuf->print("!OK,ResetPos\n");
		} else if(cmds_len == 1 && strcmp(cmds[0], "QueryDistanceCM") == 0) {
			double cm = ultraSensor.distanceCm();
			cmdbuf->print("!OK,QueryDistanceCM,%f\n", cm);
		} else {
			cmdbuf->print("!ERR,UnknownCmd\n");
		}
	}
}

// function for custom printf library to print bytes
void _putchar(char c) {
	Serial.write(c);
}
