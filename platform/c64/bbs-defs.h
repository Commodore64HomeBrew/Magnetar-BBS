/**
 * \file
 *         bbsefs.h - Contiki BBS functions and types - header file
 * \author
 *         (c) 2009-2015 by Niels Haedecke <n.haedecke@unitybox.de>
 */

#ifndef __BBSDEFS_H_
#define __BBSDEFS_H_

#define BBS_STRING_VERSION "1.0"
#define BBS_COPYRIGHT_STRING "\n\r          Magnetar BBS 1.0 \n\r        2018-2026 K. Casteels\n\r        2009-2015 N. Haedecke\n\r        2003-2013 A. Dunkels\n\r"

#define BBS_ENCODING_STRING "\n\rpetscii - 80col (8)\n\rpetscii - 40col (4)\n\rpetscii - 22col (2)\n\rascii w/ echo   (e)\n\rlinux or vt100  (l)\n\r\n\r>  "

#define BBS_MODE_SHELL          0
#define BBS_MODE_CONSOLE        1

#define BBS_MAX_BOARDS          8
#define BBS_MAX_MSGLINES        20
#define BBS_80_COL             	78
#define BBS_40_COL	           	38
#define BBS_22_COL	           	20
/* One telnet line + NUL; 128 is enough for BBS commands and wrapped post lines. */
#define TELNETD_CONF_LINELEN 	 255
#define TELNETD_CONF_NUMLINES 	25

/* Stream chunk size; movie UI caps speed at 10. */
#define MAX_STREAM_SPEED        10

/* Outbound queue; must exceed TCP MSS for smooth output on uIP builds. */
#ifndef BBS_BUFFER_SIZE
/* Keep ring size equal to max post payload to avoid truncation surprises. */
#define BBS_BUFFER_SIZE    	1500
#endif

#ifdef BBS_SERIAL_TRANSPORT
/* Raw serial: small chunks + few passes per tick avoid overrunning modem/bridge FIFOs. */
#ifndef TELNETD_SERIAL_TX_CHUNK
#define TELNETD_SERIAL_TX_CHUNK 9u
#endif
#endif
/* In-memory post body (lines appended until /s); separate from wire buffer. */
#define BBS_POST_BUFFER_SIZE    1500

/* Min size for buffers passed to file_path() (boost paths need headroom). */
#define BBS_FILE_PATH_BUFLEN    48

#define BBS_SESSION_TIMEOUT (CLOCK_SECOND * 3600)
#define BBS_LOGIN_TIMEOUT   (CLOCK_SECOND * 60)
#define BBS_IDLE_TIMEOUT    (CLOCK_SECOND * 120)



#define BBS_PET80_SUFFIX       "-c"
#define BBS_PET40_SUFFIX       "-c"
#define BBS_PET22_SUFFIX       "-v"
#define BBS_ASCII_SUFFIX       "-a"
//#define BBS_BASH_SUFFIX       "-b"
#define BBS_PREFIX_SUB         "s-"
#define BBS_PREFIX_USER        "u-"


#define BBS_BANNER_LOGIN       "login"
#define BBS_BANNER_LOGOUT      "logout"
#define BBS_BANNER_LOGO        "logo"
#define BBS_BANNER_MENU        "menu"
#define BBS_BANNER_SUBS        "subs"
#define BBS_BANNER_INFO        "info"
#define BBS_BANNER_MOVIE       "movie"


/* Must match on-disk bbs-stats layout; do not shrink without a migration path. */
#define BBS_STATS_DAYS			77
#define BBS_STATS_USRS			5
#define BBS_STATS_FILE         "bbs-stats"

#define BBS_LOG_FILE           "bbs-log"

#define BBS_CFG_FILE           "bbs-cfg"
#define BBS_SETUP_FILE         "bbs-setup"

//#define BBS_STRING_BOARDINFO "-id- -------board------- -acl- -msgs-"
//#define BBS_STRING_LINEMAX "  enter message (max. 40 chars per line)"
#define BBS_STRING_EDITH40 "---------+---------+---------+---------+"
#define BBS_STRING_EDITH22 "----------+----------+"

#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5
#define STATE_CLOSE  6

#define STATUS_UNLOCK	0
#define STATUS_HANDLE	1
#define STATUS_PASSWD	2
#define STATUS_NEWUSR	3
#define STATUS_CONFUSR	4
#define STATUS_STATS	5

#define STATUS_LOCK	6
#define STATUS_SUBJ	7
#define STATUS_POST	8
#define STATUS_READ	9
#define STATUS_STREAM  10
#define STATUS_XFER    11

/* File transfer area (under board.transfer_prefix). */
#define BBS_XFER_MAX_FILES  4
#define BBS_XFER_NAME_LEN   17
#define BBS_XFER_PATH_LEN   28
#define BBS_XFER_BLOCK      128
#define BBS_XFER_RBUF       132



#define PETSCII_LOWER   "\x0e"
#define PETSCII_WHITE   "\x05"
#define ISO_nl       	  0x0a
#define ISO_cr       	  0x0d
#define PETSCII_DEL  	  0x14
#define PETSCII_DOWN 	  0x11
#define PETSCII_UP   	  0x91
#define PETSCII_LEFT 	  0x9d
#define PETSCII_RIGHT 	0x1d
#define PETSCII_SPACE 	0x20
#define PETSCII_CLRSCN  0x93
#define PETSCII_HOME    0x13 
#define PETSCII_REVON   0x12
#define PETSCII_REVOFF	0x92

/* PETSCII colour / mode bytes: zero terminal width (banner wrap + telnet soft wrap). */
#define BBS_PETSCII_ATTR0(c) ( \
	(unsigned char)(c) == 0x05u || (unsigned char)(c) == 0x1cu || \
	(unsigned char)(c) == 0x1eu || (unsigned char)(c) == 0x1fu || \
	(unsigned char)(c) == PETSCII_REVON || (unsigned char)(c) == PETSCII_REVOFF || \
	(unsigned char)(c) == 0x81u || (unsigned char)(c) == 0x90u || \
	((unsigned char)(c) >= 0x95u && (unsigned char)(c) <= 0x9cu) || \
	(unsigned char)(c) == 0x9eu || (unsigned char)(c) == 0x9fu)

#define poke(A,X) (*(unsigned short *)A) = (X)
#define peek(A) (*(unsigned short *)A)

typedef struct {
  unsigned char  board_name[40];
  unsigned short telnet_port;
  unsigned char max_boards;
  unsigned char subs_device;
  unsigned char subs_prefix[10];
  unsigned char sys_device;
  unsigned char sys_prefix[10];
  unsigned char user_device;
  unsigned char user_prefix[10];
  unsigned char userstats_device;
  unsigned char userstats_prefix[10];
  unsigned char media_device;
  unsigned char media_prefix[10];
  unsigned char transfer_device;
  unsigned char transfer_prefix[10];
  unsigned char sub_names[9][20];
  unsigned char dir_boost; 
} BBS_BOARD_REC;


typedef struct {
  unsigned short msg_id[BBS_MAX_BOARDS+1];
} BBS_CONFIG_REC;

typedef struct {  
  unsigned char  user_name[12];
  unsigned char  user_pwd[20];
  unsigned char  access_req;
} BBS_USER_REC;

typedef struct {
  unsigned short num_calls;
  unsigned short num_msgs;
  unsigned short current_msg[BBS_MAX_BOARDS+1];
} BBS_USER_STATS;

typedef struct {
  unsigned char last_callers[BBS_STATS_USRS][12];
  unsigned char caller_ptr;
  unsigned short total_calls;
  unsigned short total_msgs;
  unsigned short daily_calls[BBS_STATS_DAYS];
  unsigned short daily_msgs[BBS_STATS_DAYS];
  unsigned char day_ptr;
} BBS_SYSTEM_STATS;

typedef struct {
  unsigned char status;
  unsigned char login;
  unsigned char board_id;
  unsigned char encoding;
  unsigned char echo;
  unsigned char wrap;
  unsigned char width;
  unsigned char lines;
  unsigned char speed;
  unsigned short msg_size;
  unsigned char prompt[40];
  unsigned char encoding_suffix[3];
} BBS_STATUS_REC;

typedef struct {
  unsigned short year;
  unsigned char month;
  unsigned char day;
  unsigned char hour;
  unsigned char minute;
} BBS_TIME_REC;


typedef struct {
  unsigned char bufmem[BBS_BUFFER_SIZE];
  unsigned int head; /* next byte to send (physical index) */
  unsigned int used; /* pending outbound bytes (may wrap) */
  unsigned int size;
} BBS_BUFFER;

#endif /* __BBSDEFS_H_ */
