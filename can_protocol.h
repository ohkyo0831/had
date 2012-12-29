#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__


enum msg_type_command
{
	MSG_COMMAND_STATUS,		// 0
	MSG_COMMAND_PING,		// 1
	MSG_COMMAND_RESET,		// 2
	MSG_COMMAND_RELAIS,		// 3
	MSG_COMMAND_EEPROM_SET,	// 4
	MSG_COMMAND_EEPROM_GET,	// 5
	MSG_COMMAND_HR20_SET_T,	// 6
	MSG_COMMAND_HR20_SET_MODE_MANU,	// 7
	MSG_COMMAND_HR20_SET_MODE_AUTO,	// 8
	MSG_COMMAND_HR20_SET_TIME,	// 9
	MSG_COMMAND_HR20_SET_DATE,	// 10
	MSG_COMMAND_MPD,// 11
	MSG_COMMAND_GET_STATUS, // 12 0C
	MSG_COMMAND_BOOTLOADER,
	MSG_COMMAND_HR20_GET_TIMER,
	MSG_COMMAND_HR20_SET_TIMER
};

enum msg_type_eeprom
{
	MSG_EEPROM_ID,
	MSG_EEPROM_RELAIS1,
	MSG_EEPROM_RELAIS2,
	MSG_EEPROM_RELAIS3,
	MSG_EEPROM_RELAIS4,
	MSG_EEPROM_RELAIS5, // 5
	MSG_EEPROM_RELAIS6,
	MSG_EEPROM_UART_MASTER,
	MSG_EEPROM_BANDGAP // 8
};

enum msg_type_mpd
{
	MSG_MPD_PLAY,
	MSG_MPD_PAUSE,
	MSG_MPD_NEXT,
	MSG_MPD_PREV,
	MSG_MPD_RANDOM,
	MSG_MPD_ID
};

enum msg_type_status
{
	MSG_STATUS_POWERUP			= 0,
	MSG_STATUS_RELAIS			= 1,
	MSG_STATUS_UPTIME			= 2,
	MSG_STATUS_EEPROM_RELAIS1	= 3,
	MSG_STATUS_EEPROM_RELAIS2	= 4,
	MSG_STATUS_EEPROM_RELAIS3	= 5,
	MSG_STATUS_EEPROM_RELAIS4	= 6,
	MSG_STATUS_EEPROM_RELAIS5	= 7,
	MSG_STATUS_EEPROM_RELAIS6	= 8,

	MSG_STATUS_HR20_TEMPS		= 9,
	MSG_STATUS_HR20_VALVE_VOLT	= 10,
	MSG_STATUS_HR20_AUTO_TEMPERATURE = 11,
	MSG_STATUS_HR20_EEPROM		= 12,
	MSG_STATUS_HR20_TIMER		= 13
};

enum node_mode
{
	MODE_HR20,
	MODE_UART_MASTER,
	MODE_HEIZUNG,
	MODE_HEIZUNG_UART_MASTER
};

#define MPD_ADDRESS 200
#endif
