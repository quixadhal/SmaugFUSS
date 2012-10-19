/****************************************************************************
 *                   ^     +----- |  / ^     ^ |     | +-\                  *
 *                  / \    |      | /  |\   /| |     | |  \                 *
 *                 /   \   +---   |<   | \ / | |     | |  |                 *
 *                /-----\  |      | \  |  v  | |     | |  /                 *
 *               /       \ |      |  \ |     | +-----+ +-/                  *
 ****************************************************************************
 * AFKMud Copyright 1997-2006 by Roger Libiez (Samson),                     *
 * Levi Beckerson (Whir), Michael Ward (Tarl), Erik Wolfe (Dwip),           *
 * Cameron Carroll (Cam), Cyberfox, Karangi, Rathian, Raine, and Adjani.    *
 * All Rights Reserved.                                                     *
 * Registered with the United States Copyright Office. TX 5-877-286         *
 *                                                                          *
 * External contributions from Xorith, Quixadhal, Zarius, and many others.  *
 *                                                                          *
 * Original SMAUG 1.4a written by Thoric (Derek Snider) with Altrag,        *
 * Blodkai, Haus, Narn, Scryn, Swordbearer, Tricops, Gorog, Rennard,        *
 * Grishnakh, Fireblade, and Nivek.                                         *
 *                                                                          *
 * Original MERC 2.1 code by Hatchet, Furey, and Kahn.                      *
 *                                                                          *
 * Original DikuMUD code by: Hans Staerfeldt, Katja Nyboe, Tom Madsen,      *
 * Michael Seifert, and Sebastian Hammer.                                   *
 ****************************************************************************
 *                       Intermud-3 Network Module                          *
 ****************************************************************************/

/*
 * Copyright (c) 2000 Fatal Dimensions
 * 
 * See the file "LICENSE" or information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * 
 */

/* Ported to Smaug 1.4a by Samson of Alsherok.
 * Consolidated for cross-codebase compatibility by Samson of Alsherok.
 * Modifications and enhancements to the code
 * Copyright (c)2001-2003 Roger Libiez ( Samson )
 * Registered with the United States Copyright Office
 * TX 5-562-404
 *
 * I condensed the 14 or so Fatal Dimensions source code files into this
 * one file, because I for one find it far easier to maintain when all of
 * the functions are right here in one file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fnmatch.h>
#include <sys/time.h>
#include <time.h>
#if defined(sun)
#include <strings.h>
#endif
#if defined(I3CIRCLE)
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpreter.h"
#include "i3.h"
time_t current_time;
#endif

#if defined(I3SMAUG) || defined(I3CHRONICLES)
#include "mud.h"
#ifdef I3CHRONICLES
#include "factions.h"
#include "files.h"
#include "i3.h"
#endif
#endif
#if defined(I3ROM) || defined(I3MERC) || defined(I3UENVY)
#include "merc.h"
#ifdef I3ROM
#include "tables.h"
#endif
#endif
#ifdef I3ACK
#include "ack.h"
#endif

#define I3_NO_ANSI_BLEED 1
#define I3_FILTER_CRLF 0
#define HANDLE_ESCAPES_IN_STRREP 0
#define NEW_COLOR_CODE 1

/* Global variables for I3 */
char I3_input_buffer[IPS];
char I3_output_buffer[OPS];
char I3_currentpacket[IPS];
bool packetdebug = FALSE;  /* Packet debugging toggle, can be turned on to check outgoing packets */
long I3_input_pointer = 0;
long I3_output_pointer = 4;
#define I3_THISMUD (this_i3mud->name)
//char *I3_THISMUD;
char *I3_ROUTER_NAME;
const char *manual_router;
int I3_socket;
int i3wait; /* Number of game loops to wait before attempting to reconnect when a socket dies */
int i3timeout; /* Number of loops to wait before giving up on an initial router connection */
time_t ucache_clock; /* Timer for pruning the ucache */
long bytes_received;
long bytes_sent;
#ifdef I3_WILEY
time_t i3_time;  /* Current clock time for the client */
#else
#define i3_time current_time
#endif

I3_MUD *this_i3mud = NULL;
I3_MUD *first_mud;
I3_MUD *last_mud;

I3_CHANNEL *first_I3chan;
I3_CHANNEL *last_I3chan;
I3_BAN *first_i3ban;
I3_BAN *last_i3ban;
UCACHE_DATA *first_ucache;
UCACHE_DATA *last_ucache;
ROUTER_DATA *first_router;
ROUTER_DATA *last_router;
I3_COLOR *first_i3_color;
I3_COLOR *last_i3_color;
I3_CMD_DATA *first_i3_command;
I3_CMD_DATA *last_i3_command;
I3_HELP_DATA *first_i3_help;
I3_HELP_DATA *last_i3_help;

void i3_printf( CHAR_DATA * ch, const char *fmt, ... ) __attribute__ ( ( format( printf, 2, 3 ) ) );
void i3_page_printf( CHAR_DATA * ch, const char *fmt, ... ) __attribute__ ( ( format( printf, 2, 3 ) ) );
void i3_bug( const char *format, ... ) __attribute__ ( ( format( printf, 1, 2 ) ) );
void i3_log( const char *format, ... ) __attribute__ ( ( format( printf, 1, 2 ) ) );
I3_HEADER *i3_get_header( char **pps );
void i3_send_channel_listen( I3_CHANNEL * channel, bool lconnect );
void i3_write_channel_config( void );
const char *i3_funcname( I3_FUN * func );
I3_FUN *i3_function( const char *func );
void i3_save_config( void );
void i3_close_connection( bool reconnect );
char *i3_rank_buffer( CHAR_DATA * ch );
char *i3_escape_url( const char *txt );
char *i3_escape_name( const char *ps );

#define I3KEY( literal, field, value ) \
if( !strcasecmp( word, literal ) )     \
{                                      \
   field = value;                      \
   fMatch = TRUE;                      \
   break;                              \
}

const char * perm_names[] = {
   "Notset", "None", "Mort", "Imm", "Admin", "Imp"
};

/*******************************************
 * String buffering and logging functions. *
 ******************************************/

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 *
 * Renamed so it can play itself system independent.
 * Samson 10-12-03
 */
size_t i3_strlcpy( char *dst, const char *src, size_t siz )
{
   register char *d = dst;
   register const char *s = src;
   register size_t n = siz;

   /*
    * Copy as many bytes as will fit 
    */
   if( n != 0 && --n != 0 )
   {
      do
      {
         if( ( *d++ = *s++ ) == 0 )
            break;
      }
      while( --n != 0 );
   }

   /*
    * Not enough room in dst, add NUL and traverse rest of src 
    */
   if( n == 0 )
   {
      if( siz != 0 )
         *d = '\0';  /* NUL-terminate dst */
      while( *s++ )
         ;
   }
   return ( s - src - 1 ); /* count does not include NUL */
}

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(initial dst) + strlen(src); if retval >= siz,
 * truncation occurred.
 *
 * Renamed so it can play itself system independent.
 * Samson 10-12-03
 */
size_t i3_strlcat( char *dst, const char *src, size_t siz )
{
   register char *d = dst;
   register const char *s = src;
   register size_t n = siz;
   size_t dlen;

   /*
    * Find the end of dst and adjust bytes left but don't go past end 
    */
   while( n-- != 0 && *d != '\0' )
      d++;
   dlen = d - dst;
   n = siz - dlen;

   if( n == 0 )
      return ( dlen + strlen( s ) );
   while( *s != '\0' )
   {
      if( n != 1 )
      {
         *d++ = *s;
         n--;
      }
      s++;
   }
   *d = '\0';
   return ( dlen + ( s - src ) );   /* count does not include NUL */
}

bool i3_str_prefix( const char *astr, const char *bstr )
{
   if( !astr )
   {
      i3_bug( "Strn_cmp: null astr." );
      return TRUE;
   }

   if( !bstr )
   {
      i3_bug( "Strn_cmp: null bstr." );
      return TRUE;
   }

   for( ; *astr; astr++, bstr++ )
   {
      if( LOWER( *astr ) != LOWER( *bstr ) )
         return TRUE;
   }
   return FALSE;
}

#ifdef I3_WILEYMUD
const char *i3_one_argument( const char *argument, char *arg_first )
{
   char cEnd;
   int count;

   count = 0;

   if( arg_first )
      arg_first[0] = '\0';

   if( !argument || argument[0] == '\0' )
      return NULL;

   while( isspace( *argument ) )
      argument++;

   cEnd = ' ';
   if( *argument == '\'' || *argument == '"' )
      cEnd = *argument++;

   while( *argument != '\0' && ++count <= SMST-1 )
   {
      if( *argument == cEnd )
      {
         argument++;
         break;
      }

      if( arg_first )
         *arg_first++ = *argument++;
      else
         argument++;
   }

   if( arg_first )
      *arg_first = '\0';

   while( isspace( *argument ) )
      argument++;

   return argument;
}
#else
#define i3_one_argument(a, b) one_argument((a), (b))
#endif

/* Generic log function which will route the log messages to the appropriate system logging function */
void i3_log( const char *format, ... )
{
   char buf[LGST], buf2[LGST];
#ifdef I3_WILEY
   char *strtime;
#endif
   va_list ap;

   va_start( ap, format );
   vsnprintf( buf, LGST, format, ap );
   va_end( ap );

   snprintf( buf2, LGST, "I3: %s", buf );

#ifndef I3CIRCLE
#ifdef I3_WILEY
   strtime = ctime( &i3_time );
   fprintf( stderr, "%s :: %s\n", strtime, buf2 );

   log_info( "%s", buf2 );
#else
   log_string( buf2 );
#endif
#else
   basic_mud_log( buf2 );
#endif
   return;
}

/* Generic bug logging function which will route the message to the appropriate function that handles bug logs */
void i3_bug( const char *format, ... )
{
   char buf[LGST];
#ifndef I3_WILEY
   char buf2[LGST];
#endif
   va_list ap;

   va_start( ap, format );
   vsnprintf( buf, LGST, format, ap );
   va_end( ap );

   snprintf( buf2, LGST, "I3: %s", buf );

#if defined(I3SMAUG) || defined(I3CHRONICLES)
   bug( "%s", buf2 );
#elif defined(I3CIRCLE)
   basic_mud_log( buf2 );
#elif defined(I3_WILEY)
   log_error( "%s", buf );
#else
   bug( buf2, 0 );
#endif
   return;
}

/*
   Original Code from SW:FotE 1.1
   Reworked strrep function. 
   Fixed a few glaring errors. It also will not overrun the bounds of a string.
   -- Xorith
*/
char *i3_strrep( const char *src, const char *sch, const char *rep )
{
   int src_len = strlen( src ), sch_len = strlen( sch ), rep_len = strlen( rep ), src_p, offset, dest_p;
   static char dest[LGST];
   bool searching = FALSE;

   dest[0] = '\0';
   for( src_p = 0, dest_p = 0; src_p < src_len; src_p++, dest_p++ )
   {
      if( src[src_p] == sch[0] )
      {
         searching = TRUE;
         for( offset = 0; offset < sch_len; offset++ )
            if( src[src_p + offset] != sch[offset] )
               searching = FALSE;

         if( searching )
         {
            for( offset = 0; offset < rep_len; offset++, dest_p++ )
            {
               if( dest_p == ( LGST - 1 ) )
               {
                  dest[dest_p] = '\0';
                  return dest;
               }
#if HANDLE_ESCAPES_IN_STRREP
               if( src[src_p - 1] == sch[0] )
               {
                  if( rep[0] == '\033' )
                  {
                     if( offset < sch_len )
                     {
                        if( offset == 0 )
                           dest[dest_p - 1] = sch[offset];
                        else
                           dest[dest_p] = sch[offset];
                     }
                     else
                        offset = rep_len;
                  }
                  else
                  {
                     if( offset == 0 )
                        dest[dest_p - 1] = rep[offset];
                     dest[dest_p] = rep[offset];
                  }
               }
               else
#endif
                  dest[dest_p] = rep[offset];
            }
            src_p += sch_len - 1;
            dest_p--;
            searching = FALSE;
            continue;
         }
      }
      if( dest_p == ( LGST - 1 ) )
      {
         dest[dest_p] = '\0';
         return dest;
      }
      dest[dest_p] = src[src_p];
   }
   dest[dest_p] = '\0';
   return dest;
}

char *i3_strip_colors( const char *txt )
{
   I3_COLOR *color;
   static char tbuf[LGST];

   i3_strlcpy( tbuf, txt, LGST );

   for( color = first_i3_color; color; color = color->next )
      i3_strlcpy( tbuf, i3_strrep( tbuf, color->i3fish, "" ), LGST );

   for( color = first_i3_color; color; color = color->next )
      i3_strlcpy( tbuf, i3_strrep( tbuf, color->i3tag, "" ), LGST );

   for( color = first_i3_color; color; color = color->next )
      i3_strlcpy( tbuf, i3_strrep( tbuf, color->mudtag, "" ), LGST );

   return tbuf;
}

char *i3_mudtofish( const char *txt )
{
   I3_COLOR *color;
   static char tbuf[LGST];
#if NEW_COLOR_CODE
   char first_chars[SMST];
   char check[2];
   const char *src_p;
   char *dest_p;
   bool match_found = FALSE;
#endif

   *tbuf = '\0';
   if( !txt || *txt == '\0' )
      return tbuf;

#if !NEW_COLOR_CODE
    i3_strlcpy( tbuf, txt, LGST );
    for( color = first_i3_color; color; color = color->next )
        i3_strlcpy( tbuf, i3_strrep( tbuf, color->mudtag, color->i3fish ), LGST );
#else
   /*
    * The above code won't work properly, because of the nature of Smaug color
    * codes.  The characters are doubled for escape, but strrep won't take that
    * into account, and since it's a loop, you don't know the order things get
    * applied.  For example, the string "&&blue" should come out as the text
    * "&blue", because the & is escaped.  However, if the &b gets matched first,
    * it will be replaced and you'll have &ESC[3mlue.  If the escape gets matched
    * first, you'll get &blue, but that may then be re-matched to ESC[3mlue.
    *
    * The only way to do this right, is to scan the string ONCE, left to right,
    * and look for potential matches as we go.  Once we find one, we have to then
    * skip the entire match, so we don't find sub-matches, as in the above example.
    */

    *first_chars = '\0';
    check[0] = check[1] = '\0';
    memset(tbuf, '\0', LGST);
    src_p = txt;
    dest_p = tbuf;

    for( color = first_i3_color; color; color = color->next )
    {
        /* First we build the set of leading characters to match, to optimize */
        check[0] = color->mudtag[0];
        if(!strpbrk(first_chars, check))
        {
            strcat(first_chars, check);
        }
    }
    for(src_p = txt, dest_p = tbuf; *src_p && strlen(tbuf) <= LGST; )
    {
        if(strpbrk(src_p, first_chars) == src_p)
        {
            match_found = FALSE;
            if(strpbrk(src_p+1, first_chars) == src_p+1)
            {
                /* Special case:  We found our token escaped, so that
                 * means just send the single version straight out and
                 * skip ahead, thus making it have no other meaning.
                 */
                match_found = TRUE;
                *dest_p++ = *src_p++;
                src_p++;
            }
            else
            {
                /* We found a partial match RIGHT HERE */
                for( color = first_i3_color; color; color = color->next )
                {
                    if(!strncmp(src_p, color->mudtag, strlen(color->mudtag)))
                    {
                        /* Exact match found, replace it! */
                        match_found = TRUE;
                        src_p += strlen(color->mudtag);
                        strcat(dest_p, color->i3fish);
                        dest_p += strlen(color->i3fish);
                        break;
                    }
                }
            }
            if(!match_found)
            {
                /* No code matched, so just copy and move on */
                *dest_p++ = *src_p++;
            }
        }
        else
        {
            *dest_p++ = *src_p++;
        }
    }
#endif
    return tbuf;
}

char *i3_fishtomud( CHAR_DATA * ch, const char *txt )
{
   I3_COLOR *color;
   static char tbuf[LGST];
#if NEW_COLOR_CODE
   const char *src_p;
   char *dest_p;
   bool match_found = FALSE;
#endif

   *tbuf = '\0';
   if( !txt || *txt == '\0' )
      return tbuf;

#if !NEW_COLOR_CODE
   if( I3IS_SET( I3FLAG( ch ), I3_COLORFLAG ) )
   {
      i3_strlcpy( tbuf, txt, LGST );
      for( color = first_i3_color; color; color = color->next )
         i3_strlcpy( tbuf, i3_strrep( tbuf, color->i3fish, color->mudtag ), LGST );
   }
   else
      i3_strlcpy( tbuf, i3_strip_colors( txt ), LGST );
#else
    memset(tbuf, '\0', LGST);
    src_p = txt;
    dest_p = tbuf;

    for(src_p = txt, dest_p = tbuf; *src_p && strlen(tbuf) <= LGST; )
    {
        if(src_p[0] == '%' && src_p[1] == '^') /* Start of a Pinkfish sequence */
        {
            if(src_p[2] == '%' && src_p[3] == '^') /* Special case of a Pinkfish escape sequence */
            {
                match_found = TRUE;
                *dest_p++ = *src_p++;
                *dest_p++ = *src_p++;
                src_p += 2;
            }
            else
            {
                match_found = FALSE;
                for( color = first_i3_color; color; color = color->next )
                {
                    if(!strncmp(src_p, color->i3fish, strlen(color->i3fish)))
                    {
                        /* Exact match found, replace it! */
                        match_found = TRUE;
                        src_p += strlen(color->i3fish);
                        if( I3IS_SET( I3FLAG( ch ), I3_COLORFLAG ) )
                        {
                            strcat(dest_p, color->mudtag);
                            dest_p += strlen(color->mudtag);
                        }
                        break;
                    }
                }
                if(!match_found)
                {
                    /* Unrecognized Pinkfish code, copy it out and hope for the best */
                    *dest_p++ = *src_p++;
                    *dest_p++ = *src_p++;
                }
            }
        }
        else
        {
            *dest_p++ = *src_p++;
        }
    }
#endif

   return tbuf;
}

char *i3_codetofish( const char *txt )
{
   I3_COLOR *color;
   static char tbuf[LGST];

   *tbuf = '\0';
   if( !txt || *txt == '\0' )
      return tbuf;

   i3_strlcpy( tbuf, txt, LGST );
   for( color = first_i3_color; color; color = color->next )
      i3_strlcpy( tbuf, i3_strrep( tbuf, color->i3tag, color->i3fish ), LGST );

   return tbuf;
}

char *i3_codetomud( CHAR_DATA * ch, const char *txt )
{
   I3_COLOR *color;
   static char tbuf[LGST];

   *tbuf = '\0';
   if( !txt || *txt == '\0' )
      return tbuf;

   if( I3IS_SET( I3FLAG( ch ), I3_COLORFLAG ) )
   {
      i3_strlcpy( tbuf, txt, LGST );
      for( color = first_i3_color; color; color = color->next )
         i3_strlcpy( tbuf, i3_strrep( tbuf, color->i3tag, color->mudtag ), LGST );
   }
   else
      i3_strlcpy( tbuf, i3_strip_colors( txt ), LGST );

   return tbuf;
}

/********************************
 * User level output functions. *
 *******************************/

/* Generic substitute for send_to_char with color support */
void i3_to_char( const char *txt, CHAR_DATA * ch )
{
   char buf[LGST];
   //char buf2[LGST];

   i3_strlcpy( buf, i3_fishtomud( ch, txt ), LGST );
   //i3_strlcpy( buf2, i3_codetomud( ch, buf ), LGST );

#ifdef I3_NO_ANSI_BLEED
   i3_strlcat( buf, i3_fishtomud( ch, "%^RESET%^" ), LGST );
#endif
#if defined(I3SMAUG) || defined(I3CHRONICLES)
   send_to_char_color( buf, ch );
#elif defined(I3CIRCLE)
#if _CIRCLEMUD < CIRCLEMUD_VERSION(3,0,21)
   send_to_char( buf, ch );
#else
   send_to_char( ch, "%s", buf );
#endif
#else
   send_to_char( buf, ch );
#endif
   return;
}

/* Generic substitute for ch_printf that passes to i3_to_char for color support */
void i3_printf( CHAR_DATA * ch, const char *fmt, ... )
{
   char buf[LGST];
   va_list args;

   va_start( args, fmt );
   vsnprintf( buf, LGST, fmt, args );
   va_end( args );

   i3_to_char( buf, ch );
}

/* Generic send_to_pager type function to send to the proper code for each codebase */
void i3_send_to_pager( const char *txt, CHAR_DATA * ch )
{
   char buf[LGST];
   //char buf2[LGST];

   i3_strlcpy( buf, i3_fishtomud( ch, txt ), LGST );
   //i3_strlcpy( buf2, i3_codetomud( ch, buf ), LGST );
   
#if defined(I3SMAUG) || defined(I3CHRONICLES)
#ifdef I3_NO_ANSI_BLEED
   i3_strlcat( buf, i3_fishtomud( ch, "%^RESET%^" ), LGST );
#endif
   send_to_pager_color( buf, ch );
#elif defined(I3ROM)
#ifdef I3_NO_ANSI_BLEED
   i3_strlcat( buf, i3_fishtomud( ch, "%^RESET%^" ), LGST );
#endif
   page_to_char( buf, ch );
#else
   i3_to_char( buf, ch );
#endif
   return;
}

/* Generic printf type function for the pager */
void i3_page_printf( CHAR_DATA * ch, const char *fmt, ... )
{
   char buf[LGST];
   va_list args;

   va_start( args, fmt );
   vsnprintf( buf, LGST, fmt, args );
   va_end( args );

   i3_send_to_pager( buf, ch );
   return;
}

/********************************
 * Low level utility functions. *
 ********************************/

int i3todikugender( int gender )
{
   int sex = 0;

   if( gender == 0 )
      sex = SEX_MALE;

   if( gender == 1 )
      sex = SEX_FEMALE;

   if( gender > 1 )
      sex = SEX_NEUTRAL;

   return sex;
}

int dikutoi3gender( int gender )
{
   int sex = 0;

   if( gender > 2 || gender < 0 )
      sex = 2; /* I3 neuter */

   if( gender == SEX_MALE )
      sex = 0; /* I3 Male */

   if( gender == SEX_FEMALE )
      sex = 1; /* I3 Female */

   return sex;
}

int get_permvalue( const char *flag )
{
   unsigned int x;

   for( x = 0; x < ( sizeof( perm_names ) / sizeof( perm_names[0] ) ); x++ )
      if( !strcasecmp( flag, perm_names[x] ) )
         return x;
   return -1;
}

/*  i3_getarg: extract a single argument (with given max length) from
 *  argument to arg; if arg==NULL, just skip an arg, don't copy it out
 */
char *i3_getarg( char *argument, char *arg, int maxlen )
{
   int len = 0;

   if( !argument || argument[0] == '\0' )
   {
      if( arg )
         arg[0] = '\0';

      return argument;
   }

   while( *argument && isspace( *argument ) )
      argument++;

   if( arg )
      while( *argument && !isspace( *argument ) && len < maxlen - 1 )
         *arg++ = *argument++, len++;
   else
      while( *argument && !isspace( *argument ) )
         argument++;

   while( *argument && !isspace( *argument ) )
      argument++;

   while( *argument && isspace( *argument ) )
      argument++;

   if( arg )
      *arg = '\0';

   return argument;
}

/* Check for a name in a list */
bool i3_has_name( char *list, const char *name )
{
   char *p;
   char arg[SMST];

   if( !list )
      return FALSE;

   p = i3_getarg( list, arg, SMST );
   while( arg[0] )
   {
      if( !strcasecmp( name, arg ) )
         return TRUE;
      p = i3_getarg( p, arg, SMST );
   }
   return FALSE;
}

/* Add a name to a list */
void i3_flagchan( char **list, const char *name )
{
   char buf[LGST];

   if( i3_has_name( *list, name ) )
      return;

   if( *list && *list[0] != '\0' )
      snprintf( buf, LGST, "%s %s", *list, name );
   else
      i3_strlcpy( buf, name, LGST );

   I3STRFREE( *list );
   *list = I3STRALLOC( buf );
}

/* Remove a name from a list */
void i3_unflagchan( char **list, const char *name )
{
   char buf[LGST], arg[SMST];
   char *p;

   buf[0] = '\0';
   p = i3_getarg( *list, arg, SMST );
   while( arg[0] )
   {
      if( strcasecmp( arg, name ) )
      {
         if( buf[0] )
            i3_strlcat( buf, " ", LGST );
         i3_strlcat( buf, arg, LGST );
      }
      p = i3_getarg( p, arg, SMST );
   }
   I3STRFREE( *list );
   *list = I3STRALLOC( buf );
}

/*
 * Returns an initial-capped string.
 */
char *i3_capitalize( const char *str )
{
   static char strcap[LGST];
   int i;

   for( i = 0; str[i] != '\0'; i++ )
      strcap[i] = tolower( str[i] );
   strcap[i] = '\0';
   strcap[0] = toupper( strcap[0] );
   return strcap;
}

/* Borrowed from Samson's new_auth snippet - checks to see if a particular player exists in the mud.
 * This is called from i3locate and i3finger to report on offline characters.
 */
bool i3exists_player( char *name )
{
#ifndef I3CIRCLE
   struct stat fst;
   char buf[SMST];
#endif

#ifdef I3CIRCLE
   return FALSE;
#else
   /*
    * Stands to reason that if there ain't a name to look at, they damn well don't exist! 
    */
   if( !name || !strcasecmp( name, "" ) )
      return FALSE;

   snprintf( buf, SMST, "%s%c/%s", PLAYER_DIR, tolower( name[0] ), i3_capitalize( name ) );

   if( stat( buf, &fst ) != -1 )
      return TRUE;
   else
      return FALSE;
#endif
}

bool i3_verify_layout( const char *fmt, int number )
{
   const char *c;
   int i = 0;

   c = fmt;
   while( ( c = strchr( c, '%' ) ) != NULL )
   {
      if( *( c + 1 ) == '%' ) /* %% */
      {
         c += 2;
         continue;
      }

      if( *( c + 1 ) != 's' ) /* not %s */
         return FALSE;

      c++;
      i++;
   }

   if( i != number )
      return FALSE;

   return TRUE;
}

/* Fixed this function yet again. If the socket is negative or 0, then it will return
 * a FALSE. Used to just check to see if the socket was positive, and that just wasn't
 * working for the way some places checked for this. Any negative value is an indication
 * that the socket never existed.
 */
bool i3_is_connected( void )
{
   if( I3_socket < 1 )
      return FALSE;

   return TRUE;
}

/*
 * Quixadhal - Replicate ^, &, or } characters found inside a word
 * that looks like a url.  Urls are not SUPPOSED to have non-encoded
 * whitespace (space should be %20), so let's hope that's what
 * we actually see.  The alternative is to scan for quotes.
 *
 * NOTE:  This is hardcoded for SmaugFUSS right now.
 *
 * SmaugFUSS uses the following color code syntax:
 *
 * First characer:
 *      &   foreground
 *      ^   background
 *      }   blinking foreground
 *
 * Second character:
 *      &   self-escape if first was &
 *      ^   self-escape if first was ^
 *      }   self-escape if first was }
 *      [   start of custom color by name
 *      Z   random
 *
 *      d   client default (reset)
 *      D   custom reset (reset + restore default page color)
 *      L   bold
 *      i   italic
 *      I   italic
 *      u   underline
 *      U   underline
 *      $   blink
 *      v   reverse (forground/background flip)
 *      V   reverse
 *      s   strikeover
 *      S   strikeover
 *
 *      x   black
 *      r   red
 *      g   green
 *      O   orange
 *      b   blue
 *      p   purple (MAGENTA)
 *      c   cyan
 *      w   grey (WHITE)
 *      z   dark grey (BOLD + BLACK)
 *      R   bright red (BOLD + RED)
 *      G   bright green (BOLD + GREEN)
 *      Y   yellow (BOLD + ORANGE)
 *      B   bright blue (BOLD + BLUE)
 *      P   pink (BOLD + MAGENTA)
 *      C   bright cyan (BOLD + CYAN)
 *      W   white (BOLD + WHITE)
 */
char *i3_escape_url( const char *txt )
{
    bool in_bare_url = FALSE;
    bool in_single_quote_url = FALSE;
    bool in_double_quote_url = FALSE;
    static char buf[MAX_STRING_LENGTH];
    char *bp;
    const char *tp;

    *buf = '\0';

    if(!txt || !*txt)
        return buf;

    if(!strcasestr(txt, "http"))
    {
        mudstrlcpy(buf, txt, MAX_STRING_LENGTH);
        return buf;
    }

    memset(buf, '\0', MAX_STRING_LENGTH);

    for(bp = buf, tp = txt; *tp && (bp - buf) < (MAX_STRING_LENGTH - 1); tp++) {
        *bp++ = *tp;
        switch(*tp) {
            default:
                break;
            case '&':  /* NORMAL, Foreground colour */
            case '^':  /* BACKGROUND colour */
            case '}':  /* BLINK Foreground colour */
                if(in_bare_url)
                {
                    /* Check for RESET before url closing characters */
                    if( *tp == '&' && *(tp+1) == 'd' && 
                        ( 
                          ( *(tp+2) == '\r' ) ||
                          ( *(tp+2) == '\n' ) ||
                          ( *(tp+2) == ' ' )  ||
                          ( *(tp+2) == '\t' ) ||
                          ( *(tp+2) == '\0' ) ) )
                    {
                        /* Don't escape this one */
                    }
                    else
                    {
                        *bp++ = *tp;
                    }
                }
                else if(in_single_quote_url)
                {
                    /* Check for RESET before url closing characters */
                    if( *tp == '&' && *(tp+1) == 'd' && 
                        ( 
                          ( *(tp+2) == '\r' ) ||
                          ( *(tp+2) == '\n' ) ||
                          ( *(tp+2) == '\'' ) ||
                          ( *(tp+2) == '\0' ) ) )
                    {
                        /* Don't escape this one */
                    }
                    else
                    {
                        *bp++ = *tp;
                    }
                }
                else if(in_double_quote_url)
                {
                    /* Check for RESET before url closing characters */
                    if( *tp == '&' && *(tp+1) == 'd' && 
                        ( 
                          ( *(tp+2) == '\r' ) ||
                          ( *(tp+2) == '\n' ) ||
                          ( *(tp+2) == '\"' ) ||
                          ( *(tp+2) == '\0' ) ) )
                    {
                        /* Don't escape this one */
                    }
                    else
                    {
                        *bp++ = *tp;
                    }
                }
                break;
            case '\'':
                if(!in_double_quote_url && !in_bare_url) {
                    if(!in_single_quote_url && (tp+1) == strcasestr((tp+1), "http"))
                    {
                        in_single_quote_url = TRUE;
                    }
                    else if(in_single_quote_url)
                    {
                        in_single_quote_url = FALSE;
                    }
                }
                break;
            case '\"':
                if(!in_single_quote_url && !in_bare_url) {
                    if(!in_double_quote_url && (tp+1) == strcasestr((tp+1), "http"))
                    {
                        in_double_quote_url = TRUE;
                    }
                    else if(in_double_quote_url)
                    {
                        in_double_quote_url = FALSE;
                    }
                }
                break;
            case 'h':
            case 'H':
                if(!in_single_quote_url && !in_double_quote_url)
                {
                    if(!in_bare_url && tp == strcasestr(tp, "http"))
                    {
                        in_bare_url = TRUE;
                    }
                }
                break;
            case ' ':
            case '\t':
                if(in_bare_url)
                {
                    in_bare_url = FALSE;
                }
                break;
            case '\r':
            case '\n':
            case '\0':
                in_bare_url = FALSE;
                in_single_quote_url = FALSE;
                in_double_quote_url = FALSE;
                break;
            case '\\':
                *bp++ = *tp++;
                break;
        }
    }
    return buf;
}

/*
 * Add backslashes in front of the " and \'s
 */
char *i3_escape( const char *ps )
{
   static char xnew[LGST];
   char *pnew = xnew;

   while( ps[0] )
   {
      if( ps[0] == '"' )
      {
         pnew[0] = '\\';
         pnew++;
      }
      if( ps[0] == '\\' )
      {
         pnew[0] = '\\';
         pnew++;
      }
#ifdef I3_FILTER_CRLF
      if( ps[0] == '\r' || ps[0] == '\n' )
      {
          ps++;
          continue;
      }
#endif
      pnew[0] = ps[0];
      pnew++;
      ps++;
   }
   pnew[0] = '\0';
   return xnew;
}

/*
 * Remove "'s at begin/end of string
 * If a character is prefixed by \'s it also will be unescaped
 */
void I3_remove_quotes( char **ps )
{
   char *ps1, *ps2;

   if( *ps[0] == '"' )
      ( *ps )++;
   if( ( *ps )[strlen( *ps ) - 1] == '"' )
      ( *ps )[strlen( *ps ) - 1] = 0;

   ps1 = ps2 = *ps;
   while( ps2[0] )
   {
      if( ps2[0] == '\\' )
      {
         ps2++;
      }
      ps1[0] = ps2[0];
      ps1++;
      ps2++;
   }
   ps1[0] = '\0';
}

/* Searches through the channel list to see if one exists with the localname supplied to it. */
I3_CHANNEL *find_I3_channel_by_localname( const char *name )
{
   I3_CHANNEL *channel = NULL;

   for( channel = first_I3chan; channel; channel = channel->next )
   {
      if( !channel->local_name )
         continue;

      if( !strcasecmp( channel->local_name, name ) )
         return channel;
   }
   return NULL;
}

/* Searches through the channel list to see if one exists with the I3 channel name supplied to it.*/
I3_CHANNEL *find_I3_channel_by_name( const char *name )
{
   I3_CHANNEL *channel = NULL;

   for( channel = first_I3chan; channel; channel = channel->next )
   {
      if( !strcasecmp( channel->I3_name, name ) )
         return channel;
   }
   return NULL;
}

/* Sets up a channel on the mud for the first time, configuring its default layout.
 * If you don't like the default layout of channels, this is where you should edit it to your liking.
 */
I3_CHANNEL *new_I3_channel( void )
{
   I3_CHANNEL *cnew;

   I3CREATE( cnew, I3_CHANNEL, 1 );
   I3LINK( cnew, first_I3chan, last_I3chan, next, prev );
   return cnew;
}

/* Deletes a channel's information from the mud. */
void destroy_I3_channel( I3_CHANNEL * channel )
{
   int x;

   if( channel == NULL )
   {
      i3_bug( "%s", "destroy_I3_channel: Null parameter" );
      return;
   }

   I3STRFREE( channel->local_name );
   I3STRFREE( channel->host_mud );
   I3STRFREE( channel->I3_name );
   I3STRFREE( channel->layout_m );
   I3STRFREE( channel->layout_e );

   for( x = 0; x < MAX_I3HISTORY; x++ )
   {
      if( channel->history[x] && channel->history[x] != '\0' )
         I3STRFREE( channel->history[x] );
   }

   I3UNLINK( channel, first_I3chan, last_I3chan, next, prev );
   I3DISPOSE( channel );
}

/* Finds a mud with the name supplied on the mudlist */
I3_MUD *find_I3_mud_by_name( const char *name )
{
   I3_MUD *mud;

   for( mud = first_mud; mud; mud = mud->next )
   {
      if( !strcasecmp( mud->name, name ) )
         return mud;
   }
   return NULL;
}

I3_MUD *new_I3_mud( char *name )
{
   I3_MUD *cnew, *mud_prev;

   I3CREATE( cnew, I3_MUD, 1 );
   cnew->name = I3STRALLOC( name );

   for( mud_prev = first_mud; mud_prev; mud_prev = mud_prev->next )
      if( strcasecmp( mud_prev->name, name ) >= 0 )
         break;

   if( !mud_prev )
      I3LINK( cnew, first_mud, last_mud, next, prev );
   else
      I3INSERT( cnew, mud_prev, first_mud, next, prev );

   return cnew;
}

void destroy_I3_mud( I3_MUD * mud )
{
   if( mud == NULL )
   {
      i3_bug( "%s", "destroy_I3_mud: Null parameter" );
      return;
   }

   I3STRFREE( mud->name );
   I3STRFREE( mud->ipaddress );
   I3STRFREE( mud->mudlib );
   I3STRFREE( mud->base_mudlib );
   I3STRFREE( mud->driver );
   I3STRFREE( mud->mud_type );
   I3STRFREE( mud->open_status );
   I3STRFREE( mud->admin_email );
   I3STRFREE( mud->telnet );
   I3STRFREE( mud->web_wrong );
   I3STRFREE( mud->banner );
   I3STRFREE( mud->web );
   I3STRFREE( mud->time );
   I3STRFREE( mud->daemon );
   I3STRFREE( mud->routerName );
   if( mud != this_i3mud )
      I3UNLINK( mud, first_mud, last_mud, next, prev );
   I3DISPOSE( mud );
}

/*
 * Returns a CHAR_DATA structure which matches the string
 *
 */
CHAR_DATA *i3_find_user( const char *name )
{
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch = NULL;

   for( d = first_descriptor; d; d = d->next )
   {
      if( ( vch = d->character ? d->character : d->original ) != NULL && !strcasecmp( CH_I3NAME( vch ), name )
          && d->connected == CON_PLAYING )
         return vch;
   }
   return NULL;
}

/* Beefed up to include wildcard ignores and user-level IP ignores.
 * Be careful when setting IP based ignores - Last resort measure, etc.
 */
bool i3_ignoring( CHAR_DATA * ch, const char *ignore )
{
   I3_IGNORE *temp;
   I3_MUD *mud;
   char *ps;
   char ipbuf[SMST], mudname[SMST];
   char atbuf[SMST];

   /*
    * Wildcard support thanks to Xorith 
    */
   for( temp = FIRST_I3IGNORE( ch ); temp; temp = temp->next )
   {
      if( !fnmatch( temp->name, ignore, 0 ) )
         return TRUE;
   }

   if( !ignore || ignore[0] == '\0' )
      return FALSE;

   /*
    * In theory, getting this far should be the result of an IP:Port ban 
    */
   i3_strlcpy(atbuf, ignore, SMST);
   ps = strchr( atbuf, '@' );

   if( ps == NULL )
      return FALSE;

   ps[0] = '\0';
   i3_strlcpy( mudname, ps + 1, SMST );

   for( mud = first_mud; mud; mud = mud->next )
   {
      if( !strcasecmp( mud->name, mudname ) )
      {
         snprintf( ipbuf, SMST, "%s:%d", mud->ipaddress, mud->player_port );
         for( temp = FIRST_I3IGNORE( ch ); temp; temp = temp->next )
         {
            if( !strcasecmp( temp->name, ipbuf ) )
               return TRUE;
         }
      }
   }
   return FALSE;
}

/* Be careful with an IP ban - Last resort measure, etc. */
bool i3_banned( const char *ignore )
{
   I3_BAN *temp;
   I3_MUD *mud;
   char *ps;
   char mudname[SMST], ipbuf[SMST];
   char atbuf[SMST];

   /*
    * Wildcard support thanks to Xorith 
    */
   for( temp = first_i3ban; temp; temp = temp->next )
   {
      if( !fnmatch( temp->name, ignore, 0 ) )
         return TRUE;
   }

   if( !ignore || ignore[0] == '\0' )
      return FALSE;

   /*
    * In theory, getting this far should be the result of an IP:Port ban 
    */
   i3_strlcpy(atbuf, ignore, SMST);
   ps = strchr( atbuf, '@' );

   if( ps == NULL )
      return FALSE;

   ps[0] = '\0';
   i3_strlcpy( mudname, ps + 1, SMST );

   for( mud = first_mud; mud; mud = mud->next )
   {
      if( !strcasecmp( mud->name, mudname ) )
      {
         snprintf( ipbuf, SMST, "%s:%d", mud->ipaddress, mud->player_port );
         for( temp = first_i3ban; temp; temp = temp->next )
         {
            if( !strcasecmp( temp->name, ipbuf ) )
               return TRUE;
         }
      }
   }
   return FALSE;
}

bool i3check_permissions( CHAR_DATA * ch, int checkvalue, int targetvalue, bool enforceequal )
{
   if( checkvalue < 0 || checkvalue > I3PERM_IMP )
   {
      i3_to_char( "Invalid permission setting.\r\n", ch );
      return FALSE;
   }

   if( checkvalue > I3PERM( ch ) )
   {
      i3_to_char( "You cannot set permissions higher than your own.\r\n", ch );
      return FALSE;
   }

   if( checkvalue == I3PERM( ch ) && I3PERM( ch ) != I3PERM_IMP && enforceequal )
   {
      i3_to_char( "You cannot set permissions equal to your own. Someone higher up must do this.\r\n", ch );
      return FALSE;
   }

   if( I3PERM( ch ) < targetvalue )
   {
      i3_to_char( "You cannot alter the permissions of someone or something above your own.\r\n", ch );
      return FALSE;
   }
   return TRUE;
}

/*
 * Read a number from a file. [Taken from Smaug's fread_number]
 */
int i3_fread_number( FILE * fp )
{
   int number;
   bool sign;
   char c;

   do
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3_fread_number: EOF encountered on read." );
         return 0;
      }
      c = getc( fp );
   }
   while( isspace( c ) );

   number = 0;

   sign = FALSE;
   if( c == '+' )
   {
      c = getc( fp );
   }
   else if( c == '-' )
   {
      sign = TRUE;
      c = getc( fp );
   }

   if( !isdigit( c ) )
   {
      i3_log( "i3_fread_number: bad format. (%c)", c );
      return 0;
   }

   while( isdigit( c ) )
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3_fread_number: EOF encountered on read." );
         return number;
      }
      number = number * 10 + c - '0';
      c = getc( fp );
   }

   if( sign )
      number = 0 - number;

   if( c == '|' )
      number += i3_fread_number( fp );
   else if( c != ' ' )
      ungetc( c, fp );

   return number;
}

/*
 * Read to end of line into static buffer [Taken from Smaug's fread_line]
 */
char *i3_fread_line( FILE * fp )
{
   char line[LGST];
   char *pline;
   char c;
   int ln;

   pline = line;
   line[0] = '\0';
   ln = 0;

   /*
    * Skip blanks.
    * Read first char.
    */
   do
   {
      if( feof( fp ) )
      {
         i3_bug( "%s", "i3_fread_line: EOF encountered on read." );
         i3_strlcpy( line, "", LGST );
         return I3STRALLOC( line );
      }
      c = getc( fp );
   }
   while( isspace( c ) );

   ungetc( c, fp );

   do
   {
      if( feof( fp ) )
      {
         i3_bug( "%s", "i3_fread_line: EOF encountered on read." );
         *pline = '\0';
         return I3STRALLOC( line );
      }
      c = getc( fp );
      *pline++ = c;
      ln++;
      if( ln >= ( LGST - 1 ) )
      {
         i3_bug( "%s", "i3_fread_line: line too long" );
         break;
      }
   }
   while( c != '\n' && c != '\r' );

   do
   {
      c = getc( fp );
   }
   while( c == '\n' || c == '\r' );

   ungetc( c, fp );
   pline--;
   *pline = '\0';

   /*
    * Since tildes generally aren't found at the end of lines, this seems workable. Will enable reading old configs. 
    */
   if( line[strlen( line ) - 1] == '~' )
      line[strlen( line ) - 1] = '\0';
   return I3STRALLOC( line );
}

/*
 * Read one word (into static buffer). [Taken from Smaug's fread_word]
 */
char *i3_fread_word( FILE * fp )
{
   static char word[SMST];
   char *pword;
   char cEnd;

   do
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3_fread_word: EOF encountered on read." );
         word[0] = '\0';
         return word;
      }
      cEnd = getc( fp );
   }
   while( isspace( cEnd ) );

   if( cEnd == '\'' || cEnd == '"' )
   {
      pword = word;
   }
   else
   {
      word[0] = cEnd;
      pword = word + 1;
      cEnd = ' ';
   }

   for( ; pword < word + SMST; pword++ )
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3_fread_word: EOF encountered on read." );
         *pword = '\0';
         return word;
      }
      *pword = getc( fp );
      if( cEnd == ' ' ? isspace( *pword ) : *pword == cEnd )
      {
         if( cEnd == ' ' )
            ungetc( *pword, fp );
         *pword = '\0';
         return word;
      }
   }

   i3_log( "%s", "i3_fread_word: word too long" );
   return NULL;
}

/*
 * Read a letter from a file. [Taken from Smaug's fread_letter]
 */
char i3fread_letter( FILE * fp )
{
   char c;

   do
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3fread_letter: EOF encountered on read." );
         return '\0';
      }
      c = getc( fp );
   }
   while( isspace( c ) );

   return c;
}

/*
 * Read to end of line (for comments). [Taken from Smaug's fread_to_eol]
 */
void i3fread_to_eol( FILE * fp )
{
   char c;

   do
   {
      if( feof( fp ) )
      {
         i3_log( "%s", "i3fread_to_eol: EOF encountered on read." );
         return;
      }
      c = getc( fp );
   }
   while( c != '\n' && c != '\r' );

   do
   {
      c = getc( fp );
   }
   while( c == '\n' || c == '\r' );

   ungetc( c, fp );
   return;
}

/******************************************
 * Packet handling and routing functions. *
 ******************************************/

/*
 * Write a string into the send-buffer. Does not yet send it.
 */
void i3_write_buffer( const char *msg )
{
   long newsize = I3_output_pointer + strlen( msg );

   if( newsize > OPS - 1 )
   {
      i3_bug( "i3_write_buffer: buffer too large (would become %ld)", newsize );
      return;
   }
   i3_strlcpy( I3_output_buffer + I3_output_pointer, msg, newsize );
   I3_output_pointer = newsize;
}

/* Use this function in place of i3_write_buffer ONLY if the text to be sent could 
 * contain color tags to parse into Pinkfish codes. Otherwise it will mess up the packet.
 */
void send_to_i3( const char *text )
{
   char buf[LGST];

   snprintf( buf, LGST, "%s", i3_mudtofish( i3_escape_url( text ) ) );
   i3_write_buffer( buf );
}

/*
 * Put a I3-header in the send-buffer. If a field is NULL it will
 * be replaced by a 0 (zero).
 */
void i3_write_header( const char *identifier, const char *originator_mudname, const char *originator_username, const char *target_mudname,
                      const char *target_username )
{
   i3_write_buffer( "({\"" );
   i3_write_buffer( identifier );
   i3_write_buffer( "\",5," );
   if( originator_mudname )
   {
      i3_write_buffer( "\"" );
      i3_write_buffer( originator_mudname );
      i3_write_buffer( "\"," );
   }
   else
      i3_write_buffer( "0," );

   if( originator_username )
   {
      i3_write_buffer( "\"" );
      i3_write_buffer( i3_escape_name(originator_username) );
      i3_write_buffer( "\"," );
   }
   else
      i3_write_buffer( "0," );

   if( target_mudname )
   {
      i3_write_buffer( "\"" );
      i3_write_buffer( target_mudname );
      i3_write_buffer( "\"," );
   }
   else
      i3_write_buffer( "0," );

   if( target_username )
   {
      i3_write_buffer( "\"" );
      i3_write_buffer( target_username );
      i3_write_buffer( "\"," );
   }
   else
      i3_write_buffer( "0," );
}

/*
 * Gets the next I3 field, that is when the amount of {[("'s and
 * ")]}'s match each other when a , is read. It's not foolproof, it
 * should honestly be some kind of statemachine, which does error-
 * checking. Right now I trust the I3-router to send proper packets
 * only. How naive :-) [Indeed Edwin, but I suppose we have little choice :P - Samson]
 *
 * ps will point to the beginning of the next field.
 *
 */
char *i3_get_field( char *packet, char **ps )
{
   int count[256];
   char has_apostrophe = 0, has_backslash = 0;
   char foundit = 0;

   bzero( count, sizeof( count ) );

   *ps = packet;
   while( 1 )
   {
      switch ( *ps[0] )
      {
         case '{':
            if( !has_apostrophe )
               count[( int )'{']++;
            break;
         case '}':
            if( !has_apostrophe )
               count[( int )'}']++;
            break;
         case '[':
            if( !has_apostrophe )
               count[( int )'[']++;
            break;
         case ']':
            if( !has_apostrophe )
               count[( int )']']++;
            break;
         case '(':
            if( !has_apostrophe )
               count[( int )'(']++;
            break;
         case ')':
            if( !has_apostrophe )
               count[( int )')']++;
            break;
         case '\\':
            if( has_backslash )
               has_backslash = 0;
            else
               has_backslash = 1;
            break;
         case '"':
            if( has_backslash )
            {
               has_backslash = 0;
            }
            else
            {
               if( has_apostrophe )
                  has_apostrophe = 0;
               else
                  has_apostrophe = 1;
            }
            break;
         case ',':
         case ':':
            if( has_apostrophe )
               break;
            if( has_backslash )
               break;
            if( count[( int )'{'] != count[( int )'}'] )
               break;
            if( count[( int )'['] != count[( int )']'] )
               break;
            if( count[( int )'('] != count[( int )')'] )
               break;
            foundit = 1;
            break;
      }
      if( foundit )
         break;
      ( *ps )++;
   }
   *ps[0] = '\0';
   ( *ps )++;
   return *ps;
}

/*
 * Writes the string into the socket, prefixed by the size.
 */
bool i3_write_packet( char *msg )
{
   int oldsize, size, check, x;
   char *s = I3_output_buffer;

   oldsize = size = strlen( msg + 4 );
   s[3] = size % 256;
   size >>= 8;
   s[2] = size % 256;
   size >>= 8;
   s[1] = size % 256;
   size >>= 8;
   s[0] = size % 256;

   /*
    * Scan for \r used in Diku client packets and change to NULL 
    */
   for( x = 0; x < oldsize + 4; x++ )
      if( msg[x] == '\r' && x > 3 )
         msg[x] = '\0';

   check = send( I3_socket, msg, oldsize + 4, 0 );

   if( !check || ( check < 0 && errno != EAGAIN && errno != EWOULDBLOCK ) )
   {
      if( check < 0 )
         i3_log( "%s", "Write error on socket." );
      else
         i3_log( "%s", "EOF encountered on socket write." );
      i3_close_connection( TRUE );
      return FALSE;
   }

   if( check < 0 )   /* EAGAIN */
      return TRUE;

   bytes_sent += check;
   if( packetdebug )
   {
      i3_log( "Size: %d. Bytes Sent: %d.", oldsize, check );
      i3_log( "Packet Sent: %s", msg + 4 );
   }
   I3_output_pointer = 4;
   return TRUE;
}

void i3_send_packet( void )
{
   i3_write_packet( I3_output_buffer );
   return;
}

/* The all important startup packet. This is what will be initially sent upon trying to connect
 * to the I3 router. It is therefore quite important that the information here be exactly correct.
 * If anything is wrong, your packet will be dropped by the router as invalid and your mud simply
 * won't connect to I3. DO NOT USE COLOR TAGS FOR ANY OF THIS INFORMATION!!!
 * Bugs fixed in this on 8-31-03 for improperly sent tags. Streamlined for less packet bloat.
 */
void i3_startup_packet( void )
{
   char s[SMST];
   char *strtime;

   if( !i3_is_connected(  ) )
      return;

   I3_output_pointer = 4;
   I3_output_buffer[0] = '\0';

   i3_log( "Sending startup_packet to %s", I3_ROUTER_NAME );

   i3_write_header( "startup-req-3", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );

   snprintf( s, SMST, "%d", this_i3mud->password );
   i3_write_buffer( s );
   i3_write_buffer( "," );
   snprintf( s, SMST, "%d", this_i3mud->mudlist_id );
   i3_write_buffer( s );
   i3_write_buffer( "," );
   snprintf( s, SMST, "%d", this_i3mud->chanlist_id );
   i3_write_buffer( s );
   i3_write_buffer( "," );
   snprintf( s, SMST, "%d", this_i3mud->player_port );
   i3_write_buffer( s );
   i3_write_buffer( ",0,0,\"" );

   i3_write_buffer( this_i3mud->mudlib );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( this_i3mud->base_mudlib );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( this_i3mud->driver );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( this_i3mud->mud_type );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( this_i3mud->open_status );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( this_i3mud->admin_email );
   i3_write_buffer( "\"," );

   /*
    * Begin first mapping set 
    */
   i3_write_buffer( "([" );
   if( this_i3mud->emoteto )
      i3_write_buffer( "\"emoteto\":1," );
   if( this_i3mud->news )
      i3_write_buffer( "\"news\":1," );
   if( this_i3mud->ucache )
      i3_write_buffer( "\"ucache\":1," );
   if( this_i3mud->auth )
      i3_write_buffer( "\"auth\":1," );
   if( this_i3mud->locate )
      i3_write_buffer( "\"locate\":1," );
   if( this_i3mud->finger )
      i3_write_buffer( "\"finger\":1," );
   if( this_i3mud->channel )
      i3_write_buffer( "\"channel\":1," );
   if( this_i3mud->who )
      i3_write_buffer( "\"who\":1," );
   if( this_i3mud->tell )
      i3_write_buffer( "\"tell\":1," );
   if( this_i3mud->beep )
      i3_write_buffer( "\"beep\":1," );
   if( this_i3mud->mail )
      i3_write_buffer( "\"mail\":1," );
   if( this_i3mud->file )
      i3_write_buffer( "\"file\":1," );
   if( this_i3mud->http )
   {
      snprintf( s, SMST, "\"http\":%d,", this_i3mud->http );
      i3_write_buffer( s );
   }
   if( this_i3mud->smtp )
   {
      snprintf( s, SMST, "\"smtp\":%d,", this_i3mud->smtp );
      i3_write_buffer( s );
   }
   if( this_i3mud->pop3 )
   {
      snprintf( s, SMST, "\"pop3\":%d,", this_i3mud->pop3 );
      i3_write_buffer( s );
   }
   if( this_i3mud->ftp )
   {
      snprintf( s, SMST, "\"ftp\":%d,", this_i3mud->ftp );
      i3_write_buffer( s );
   }
   if( this_i3mud->nntp )
   {
      snprintf( s, SMST, "\"nntp\":%d,", this_i3mud->nntp );
      i3_write_buffer( s );
   }
   if( this_i3mud->rcp )
   {
      snprintf( s, SMST, "\"rcp\":%d,", this_i3mud->rcp );
      i3_write_buffer( s );
   }
   if( this_i3mud->amrcp )
   {
      snprintf( s, SMST, "\"amrcp\":%d,", this_i3mud->amrcp );
      i3_write_buffer( s );
   }
   i3_write_buffer( "]),([" );

   /*
    * END first set of "mappings", start of second set 
    */
   if( this_i3mud->web && this_i3mud->web[0] != '\0' )
   {
      snprintf( s, SMST, "\"url\":\"%s\",", this_i3mud->web );
      i3_write_buffer( s );
   }
   strtime = ctime( &i3_time );
   strtime[strlen( strtime ) - 1] = '\0';
   snprintf( s, SMST, "\"time\":\"%s\",", strtime );
   i3_write_buffer( s );

   i3_write_buffer( "]),})\r" );
   i3_send_packet(  );
}

/* This function saves the password, mudlist ID, and chanlist ID that are used by the mud.
 * The password value is returned from the I3 router upon your initial connection.
 * The mudlist and chanlist ID values are updated as needed while your mud is connected.
 * Do not modify the file it generates because doing so may prevent your mud from reconnecting
 * to the router in the future. This file will be rewritten each time the i3_shutdown function
 * is called, or any of the id values change.
 */
void i3_save_id( void )
{
   FILE *fp;

   if( !( fp = fopen( I3_PASSWORD_FILE, "w" ) ) )
   {
      i3_log( "%s", "Couldn't write to I3 password file." );
      return;
   }

   fprintf( fp, "%s", "#PASSWORD\n" );
   fprintf( fp, "%d %d %d\n", this_i3mud->password, this_i3mud->mudlist_id, this_i3mud->chanlist_id );
   I3FCLOSE( fp );
}

/* The second most important packet your mud will deal with. If you never get this
 * coming back from the I3 router, something was wrong with your startup packet
 * or the router may be jammed up. Whatever the case, if you don't get a reply back
 * your mud won't be acknowledged as connected.
 */
void I3_process_startup_reply( I3_HEADER * header, char *s )
{
   ROUTER_DATA *router;
   I3_CHANNEL *channel;
   char *ps = s, *next_ps;

   /*
    * Recevies the router list. Nothing much to do here until there's more than 1 router. 
    */
   i3_get_field( ps, &next_ps );
   i3_log( "%s", ps );   /* Just checking for now */
   ps = next_ps;

   /*
    * Receives your mud's updated password, which may or may not be the same as what it sent out before 
    */
   i3_get_field( ps, &next_ps );
   this_i3mud->password = atoi( ps );

   i3_log( "Received startup_reply from %s", header->originator_mudname );

   i3_save_id(  );

   for( router = first_router; router; router = router->next )
   {
      if( !strcasecmp( router->name, header->originator_mudname ) )
      {
         router->reconattempts = 0;
         I3_ROUTER_NAME = router->name;
         break;
      }
   }
   i3wait = 0;
   i3timeout = 0;
   i3_log( "%s", "Intermud-3 Network connection complete." );

   for( channel = first_I3chan; channel; channel = channel->next )
   {
      if( channel->local_name && channel->local_name[0] != '\0' )
      {
         i3_log( "Subscribing to %s", channel->local_name );
         i3_send_channel_listen( channel, TRUE );
      }
   }
   return;
}

void I3_process_chanack( I3_HEADER * header, char *s )
{
   CHAR_DATA *ch;
   char *next_ps, *ps = s;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !( ch = i3_find_user( header->target_username ) ) )
      i3_log( "%s", ps );
   else
      i3_printf( ch, "&G%s\r\n", ps );
   return;
}

void I3_send_error( const char *mud, const char *user, const char *code, const char *message )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "error", I3_THISMUD, 0, mud, user );
   i3_write_buffer( "\"" );
   i3_write_buffer( code );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( i3_escape( message ) );
   i3_write_buffer( "\",0,})\r" );
   i3_send_packet(  );
}

void I3_process_error( I3_HEADER * header, char *s )
{
   CHAR_DATA *ch;
   char *next_ps, *ps = s;
   char type[SMST], error[LGST];

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( type, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   /*
    * Since VargonMUD likes to spew errors for no good reason.... 
    */
   if( !strcasecmp( header->originator_mudname, "VargonMUD" ) )
      return;

   snprintf( error, LGST, "Error: from %s to %s@%s\r\n%s: %s",
             header->originator_mudname, header->target_username, header->target_mudname, type, ps );

   if( !( ch = i3_find_user( header->target_username ) ) )
      i3_log( "%s", error );
   else
      i3_printf( ch, "&R%s\r\n", error );
}

int I3_get_ucache_gender( char *name )
{
   UCACHE_DATA *user;

   for( user = first_ucache; user; user = user->next )
   {
      if( !strcasecmp( user->name, name ) )
         return user->gender;
   }

   /*
    * -1 means you aren't in the list and need to be put there. 
    */
   return -1;
}

/* Saves the ucache info to disk because it would just be spamcity otherwise */
void I3_save_ucache( void )
{
   FILE *fp;
   UCACHE_DATA *user;

   if( !( fp = fopen( I3_UCACHE_FILE, "w" ) ) )
   {
      i3_log( "%s", "Couldn't write to I3 ucache file." );
      return;
   }

   for( user = first_ucache; user; user = user->next )
   {
      fprintf( fp, "%s", "#UCACHE\n" );
      fprintf( fp, "Name %s\n", user->name );
      fprintf( fp, "Sex  %d\n", user->gender );
      fprintf( fp, "Time %ld\n", ( long int )user->time );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

void i3_prune_ucache( void )
{
   UCACHE_DATA *ucache, *next_ucache;

   for( ucache = first_ucache; ucache; ucache = next_ucache )
   {
      next_ucache = ucache->next;

      /*
       * Info older than 30 days is removed since this person likely hasn't logged in at all 
       */
      if( i3_time - ucache->time >= 2592000 )
      {
         I3STRFREE( ucache->name );
         I3UNLINK( ucache, first_ucache, last_ucache, next, prev );
         I3DISPOSE( ucache );
      }
   }
   I3_save_ucache(  );
   return;
}

/* Updates user info if they exist, adds them if they don't. */
void I3_ucache_update( char *name, int gender )
{
   UCACHE_DATA *user;

   for( user = first_ucache; user; user = user->next )
   {
      if( !strcasecmp( user->name, name ) )
      {
         user->gender = gender;
         user->time = i3_time;
         return;
      }
   }
   I3CREATE( user, UCACHE_DATA, 1 );
   user->name = I3STRALLOC( name );
   user->gender = gender;
   user->time = i3_time;
   I3LINK( user, first_ucache, last_ucache, next, prev );

   I3_save_ucache(  );
   return;
}

void i3_send_ucache_update( const char *visname, int gender )
{
   char buf[10];

   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "ucache-update", I3_THISMUD, NULL, NULL, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( visname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( visname );
   i3_write_buffer( "\"," );
   snprintf( buf, 10, "%d", gender );
   i3_write_buffer( buf );
   i3_write_buffer( ",})\r" );
   i3_send_packet(  );

   return;
}

void i3_process_ucache_update( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char username[SMST], visname[SMST], buf[LGST];
   int sex, gender;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( username, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   gender = atoi( ps );

   snprintf( buf, LGST, "%s@%s", visname, header->originator_mudname );

   sex = I3_get_ucache_gender( buf );

   if( sex == gender )
      return;

   I3_ucache_update( buf, gender );
   return;
}

void I3_send_chan_user_req( char *targetmud, char *targetuser )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "chan-user-req", I3_THISMUD, NULL, targetmud, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( targetuser );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_chan_user_req( I3_HEADER * header, char *s )
{
   char buf[LGST];
   char *ps = s, *next_ps;
   int gender;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( buf, LGST, "%s@%s", header->target_username, I3_THISMUD );
   gender = I3_get_ucache_gender( buf );

   /*
    * Gender of -1 means they aren't in the mud's ucache table, don't waste a packet on a reply 
    */
   if( gender == -1 )
      return;

   i3_write_header( "chan-user-reply", I3_THISMUD, NULL, header->originator_mudname, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( ps );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( ps );
   i3_write_buffer( "\"," );
   snprintf( buf, LGST, "%d", gender );
   i3_write_buffer( buf );
   i3_write_buffer( ",})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_chan_user_reply( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char username[SMST], visname[SMST], buf[LGST];
   int sex, gender;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( username, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   gender = atoi( ps );

   snprintf( buf, LGST, "%s@%s", visname, header->originator_mudname );

   sex = I3_get_ucache_gender( buf );

   if( sex == gender )
      return;

   I3_ucache_update( buf, gender );
   return;
}

void I3_process_mudlist( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   I3_MUD *mud = NULL;
   char mud_name[SMST];

   i3_get_field( ps, &next_ps );
   this_i3mud->mudlist_id = atoi( ps );
   i3_save_id(  );

   ps = next_ps;
   ps += 2;

   while( 1 )
   {
      char *next_ps2;
      i3_get_field( ps, &next_ps );
      I3_remove_quotes( &ps );
      i3_strlcpy( mud_name, ps, SMST );

      ps = next_ps;
      i3_get_field( ps, &next_ps2 );

      if( ps[0] != '0' )
      {
         mud = find_I3_mud_by_name( mud_name );
         if( !mud )
            mud = new_I3_mud( mud_name );

         ps += 2;
         i3_get_field( ps, &next_ps );
         mud->status = atoi( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->ipaddress );
         mud->ipaddress = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         mud->player_port = atoi( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         mud->imud_tcp_port = atoi( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         mud->imud_udp_port = atoi( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->mudlib );
         mud->mudlib = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->base_mudlib );
         mud->base_mudlib = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->driver );
         mud->driver = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->mud_type );
         mud->mud_type = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->open_status );
         mud->open_status = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( mud->admin_email );
         mud->admin_email = I3STRALLOC( ps );
         ps = next_ps;

         i3_get_field( ps, &next_ps );

         ps += 2;
         while( 1 )
         {
            char *next_ps3;
            char key[SMST];

            if( ps[0] == ']' )
               break;

            i3_get_field( ps, &next_ps3 );
            I3_remove_quotes( &ps );
            i3_strlcpy( key, ps, SMST );
            ps = next_ps3;
            i3_get_field( ps, &next_ps3 );

            switch ( key[0] )
            {
               case 'a':
                  if( !strcasecmp( key, "auth" ) )
                  {
                     mud->auth = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  if( !strcasecmp( key, "amrcp" ) )
                  {
                     mud->amrcp = atoi( ps );
                     break;
                  }
                  break;
               case 'b':
                  if( !strcasecmp( key, "beep" ) )
                  {
                     mud->beep = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'c':
                  if( !strcasecmp( key, "channel" ) )
                  {
                     mud->channel = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'e':
                  if( !strcasecmp( key, "emoteto" ) )
                  {
                     mud->emoteto = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'f':
                  if( !strcasecmp( key, "file" ) )
                  {
                     mud->file = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  if( !strcasecmp( key, "finger" ) )
                  {
                     mud->finger = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  if( !strcasecmp( key, "ftp" ) )
                  {
                     mud->ftp = atoi( ps );
                     break;
                  }
                  break;
               case 'h':
                  if( !strcasecmp( key, "http" ) )
                  {
                     mud->http = atoi( ps );
                     break;
                  }
                  break;
               case 'l':
                  if( !strcasecmp( key, "locate" ) )
                  {
                     mud->locate = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'm':
                  if( !strcasecmp( key, "mail" ) )
                  {
                     mud->mail = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'n':
                  if( !strcasecmp( key, "news" ) )
                  {
                     mud->news = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  if( !strcasecmp( key, "nntp" ) )
                  {
                     mud->nntp = atoi( ps );
                     break;
                  }
                  break;
               case 'p':
                  if( !strcasecmp( key, "pop3" ) )
                  {
                     mud->pop3 = atoi( ps );
                     break;
                  }
                  break;
               case 'r':
                  if( !strcasecmp( key, "rcp" ) )
                  {
                     mud->rcp = atoi( ps );
                     break;
                  }
                  break;
               case 's':
                  if( !strcasecmp( key, "smtp" ) )
                  {
                     mud->smtp = atoi( ps );
                     break;
                  }
                  break;
               case 't':
                  if( !strcasecmp( key, "tell" ) )
                  {
                     mud->tell = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               case 'u':
                  if( !strcasecmp( key, "ucache" ) )
                  {
                     mud->ucache = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  if( !strcasecmp( key, "url" ) )
                  {
                     I3_remove_quotes( &ps );
                     I3STRFREE( mud->web_wrong );
                     mud->web_wrong = I3STRALLOC( ps );
                     break;
                  }
                  break;
               case 'w':
                  if( !strcasecmp( key, "who" ) )
                  {
                     mud->who = ps[0] == '0' ? 0 : 1;
                     break;
                  }
                  break;
               default:
                  break;
            }

            ps = next_ps3;
            if( ps[0] == ']' )
               break;
         }
         ps = next_ps;

         i3_get_field( ps, &next_ps );
         ps = next_ps;

      }
      else
      {
         if( ( mud = find_I3_mud_by_name( mud_name ) ) != NULL )
            destroy_I3_mud( mud );
      }
      ps = next_ps2;
      if( ps[0] == ']' )
         break;
   }
   return;
}

void i3_process_chanlist_reply( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   I3_CHANNEL *channel;
   char chan[SMST];

   i3_log( "i3_process_chanlist_reply: %s", "Got chanlist-reply packet!" );

   i3_get_field( ps, &next_ps );
   this_i3mud->chanlist_id = atoi( ps );
   i3_save_id(  );

   ps = next_ps;
   ps += 2;

   while( 1 )
   {
      char *next_ps2;

      i3_get_field( ps, &next_ps );
      I3_remove_quotes( &ps );
      i3_strlcpy( chan, ps, SMST );

      ps = next_ps;
      i3_get_field( ps, &next_ps2 );
      if( ps[0] != '0' )
      {
         if( !( channel = find_I3_channel_by_name( chan ) ) )
         {
            channel = new_I3_channel(  );
            channel->I3_name = I3STRALLOC( chan );
            i3_log( "New channel %s has been added from router %s", channel->I3_name, I3_ROUTER_NAME );
         } else {
            //i3_log( "Channel %s has been updated from router %s", channel->I3_name, I3_ROUTER_NAME );
         }

         ps += 2;
         i3_get_field( ps, &next_ps );
         I3_remove_quotes( &ps );
         I3STRFREE( channel->host_mud );
         channel->host_mud = I3STRALLOC( ps );
         ps = next_ps;
         i3_get_field( ps, &next_ps );
         channel->status = atoi( ps );
      }
      else
      {
         if( ( channel = find_I3_channel_by_name( chan ) ) != NULL )
         {
            if( channel->local_name && channel->local_name[0] != '\0' )
               i3_log( "Locally configured channel %s has been purged from router %s", channel->local_name, I3_ROUTER_NAME );
            destroy_I3_channel( channel );
            i3_write_channel_config(  );
         }
      }
      ps = next_ps2;
      if( ps[0] == ']' )
         break;
   }
   /* We should actually write this out */
   i3_log( "i3_process_chanlist_reply: %s", "Saving channel config data." );
   i3_write_channel_config(  );
   return;
}

void i3_send_channel_message( I3_CHANNEL * channel, const char *name, const char *message )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-m", I3_THISMUD, name, NULL, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( name );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( message ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void i3_send_channel_emote( I3_CHANNEL * channel, const char *name, const char *message )
{
   char buf[LGST];

   if( !i3_is_connected(  ) )
      return;

   if( strstr( message, "$N" ) == NULL )
      snprintf( buf, LGST, "$N %s", message );
   else
      i3_strlcpy( buf, message, LGST );

   i3_write_header( "channel-e", I3_THISMUD, name, NULL, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( name );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( buf ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void i3_send_channel_t( I3_CHANNEL * channel, const char *name, const char *tmud, const char *tuser, const char *msg_o, const char *msg_t, const char *tvis )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-t", I3_THISMUD, name, NULL, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( tmud );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( tuser );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( msg_o ) );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( msg_t ) );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( tvis );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

int I3_token( char type, char *string, char *oname, char *tname )
{
   char code[50];
   char *p = '\0';

   switch ( type )
   {
      default:
         code[0] = type;
         code[1] = '\0';
         return 1;
      case '$':
         i3_strlcpy( code, "$", 50 );
         break;
      case ' ':
         i3_strlcpy( code, " ", 50 );
         break;
      case 'N':  /* Originator's name */
         i3_strlcpy( code, oname, 50 );
         break;
      case 'O':  /* Target's name */
         i3_strlcpy( code, tname, 50 );
         break;
   }
   p = code;
   while( *p != '\0' )
   {
      *string = *p++;
      *++string = '\0';
   }
   return ( strlen( code ) );
}

void I3_message_convert( char *buffer, const char *txt, char *oname, char *tname )
{
   const char *point;
   int skip = 0;

   for( point = txt; *point; point++ )
   {
      if( *point == '$' )
      {
         point++;
         if( *point == '\0' )
            point--;
         else
            skip = I3_token( *point, buffer, oname, tname );
         while( skip-- > 0 )
            ++buffer;
         continue;
      }
      *buffer = *point;
      *++buffer = '\0';
   }
   *buffer = '\0';
   return;
}

char *I3_convert_channel_message( const char *message, char *sname, char *tname )
{
   static char msgbuf[LGST];

   strcpy(msgbuf, "ERROR");
   /*
    * Sanity checks - if any of these are NULL, bad things will happen - Samson 6-29-01 
    */
   if( !message )
   {
      i3_bug( "%s", "I3_convert_channel_message: NULL message!" );
      return msgbuf;
   }

   if( !sname )
   {
      i3_bug( "%s", "I3_convert_channel_message: NULL sname!" );
      return msgbuf;
   }

   if( !tname )
   {
      i3_bug( "%s", "I3_convert_channel_message: NULL tname!" );
      return msgbuf;
   }

   I3_message_convert( msgbuf, message, sname, tname );
   return msgbuf;
}

void update_chanhistory( I3_CHANNEL * channel, char *message )
{
   char msg[LGST], buf[LGST];
   struct tm *local;
   time_t t;
   int x;

   if( !channel )
   {
      i3_bug( "%s", "update_chanhistory: NULL channel received!" );
      return;
   }

   if( !message || message[0] == '\0' )
   {
      i3_bug( "%s", "update_chanhistory: NULL message received!" );
      return;
   }

   i3_strlcpy( msg, message, LGST );
   for( x = 0; x < MAX_I3HISTORY; x++ )
   {
      if( channel->history[x] == NULL )
      {
         t = time( NULL );
         local = localtime( &t );
         snprintf( buf, LGST, "&R[%-2.2d/%-2.2d %-2.2d:%-2.2d] &G%s",
                   local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, msg );
         channel->history[x] = I3STRALLOC( buf );

         if( I3IS_SET( channel->flags, I3CHAN_LOG ) )
         {
            FILE *fp;
            snprintf( buf, LGST, "%s%s.log", I3_DIR, channel->local_name );
            if( !( fp = fopen( buf, "a" ) ) )
            {
               perror( buf );
               i3_bug( "Could not open file %s!", buf );
            }
            else
            {
               fprintf( fp, "%s\n", i3_strip_colors( channel->history[x] ) );
               I3FCLOSE( fp );
            }
         }
         break;
      }

      if( x == MAX_I3HISTORY - 1 )
      {
         int y;

         for( y = 1; y < MAX_I3HISTORY; y++ )
         {
            int z = y - 1;

            if( channel->history[z] != NULL )
            {
               I3STRFREE( channel->history[z] );
               channel->history[z] = I3STRALLOC( channel->history[y] );
            }
         }

         t = time( NULL );
         local = localtime( &t );
         snprintf( buf, LGST, "&R[%-2.2d/%-2.2d %-2.2d:%-2.2d] &G%s",
                   local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, msg );
         I3STRFREE( channel->history[x] );
         channel->history[x] = I3STRALLOC( buf );

         if( I3IS_SET( channel->flags, I3CHAN_LOG ) )
         {
            FILE *fp;
            snprintf( buf, LGST, "%s%s.log", I3_DIR, channel->local_name );
            if( !( fp = fopen( buf, "a" ) ) )
            {
               perror( buf );
               i3_bug( "Could not open file %s!", buf );
            }
            else
            {
               fprintf( fp, "%s\n", i3_strip_colors( channel->history[x] ) );
               I3FCLOSE( fp );
            }
         }
      }
   }
   return;
}

/* Handles the support for channel filtering.
 * Pretty basic right now. Any truly useful filtering would have to be at the discretion of the channel owner anyway.
 */
void I3_chan_filter_m( I3_CHANNEL * channel, I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char visname[SMST], newmsg[LGST];

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( newmsg, ps, LGST );
   snprintf( newmsg, LGST, "%s%s", ps, " (filtered M)" );

   i3_write_header( "chan-filter-reply", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",({\"channel-m\",5,\"" );
   i3_write_buffer( header->originator_mudname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( header->originator_username );
   i3_write_buffer( "\",0,0,\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( visname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( newmsg );
   i3_write_buffer( "\",}),})\r" );

   i3_send_packet(  );
   return;
}

/* Handles the support for channel filtering.
 * Pretty basic right now. Any truly useful filtering would have to be at the discretion of the channel owner anyway.
 */
void I3_chan_filter_e( I3_CHANNEL * channel, I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char visname[SMST], newmsg[LGST];

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   snprintf( newmsg, LGST, "%s%s", ps, " (filtered E)" );

   i3_write_header( "chan-filter-reply", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",({\"channel-e\",5,\"" );
   i3_write_buffer( header->originator_mudname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( header->originator_username );
   i3_write_buffer( "\",0,0,\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( visname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( newmsg );
   i3_write_buffer( "\",}),})\r" );

   i3_send_packet(  );
   return;
}

/* Handles the support for channel filtering.
 * Pretty basic right now. Any truly useful filtering would have to be at the discretion of the channel owner anyway.
 */
void I3_chan_filter_t( I3_CHANNEL * channel, I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char targetmud[SMST], targetuser[SMST], message_o[LGST], message_t[LGST];
   char visname_o[SMST];

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( targetmud, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( targetuser, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   snprintf( message_o, LGST, "%s%s", ps, " (filtered T)" );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   snprintf( message_t, LGST, "%s%s", ps, " (filtered T)" );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname_o, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   i3_write_header( "chan-filter-reply", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",({\"channel-t\",5,\"" );
   i3_write_buffer( header->originator_mudname );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( header->originator_username );
   i3_write_buffer( "\",0,0,\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( targetmud );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( targetuser );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( message_o ) );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( message_t ) );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( visname_o );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( ps );
   i3_write_buffer( "\",}),})\r" );

   i3_send_packet(  );
   return;
}

void I3_process_channel_filter( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char ptype[SMST];
   I3_CHANNEL *channel = NULL;
   I3_HEADER *second_header;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      i3_log( "I3_process_channel_filter: received unknown channel (%s)", ps );
      return;
   }

   if( !channel->local_name )
      return;

   ps = next_ps;
   ps += 2;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( ptype, ps, SMST );

   second_header = i3_get_header( &ps );

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !strcasecmp( ptype, "channel-m" ) )
      I3_chan_filter_m( channel, second_header, next_ps );
   if( !strcasecmp( ptype, "channel-e" ) )
      I3_chan_filter_e( channel, second_header, next_ps );
   if( !strcasecmp( ptype, "channel-t" ) )
      I3_chan_filter_t( channel, second_header, next_ps );

   I3DISPOSE( second_header );
   return;
}

void I3_process_channel_t( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch = NULL;
   char targetmud[SMST], targetuser[SMST], message_o[LGST], message_t[LGST], buf[LGST];
   char visname_o[SMST], sname[SMST], tname[SMST], lname[SMST], tmsg[LGST], omsg[LGST];
   I3_CHANNEL *channel = NULL;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      i3_log( "I3_process_channel_t: received unknown channel (%s)", ps );
      return;
   }

   if( !channel->local_name )
      return;

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( targetmud, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( targetuser, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( message_o, ps, LGST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( message_t, ps, LGST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname_o, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( sname, SMST, "%s@%s", visname_o, header->originator_mudname );
   snprintf( tname, SMST, "%s@%s", ps, targetmud );

   snprintf( omsg, LGST, "%s", I3_convert_channel_message( message_o, sname, tname ) );
   snprintf( tmsg, LGST, "%s", I3_convert_channel_message( message_t, sname, tname ) );

   for( d = first_descriptor; d; d = d->next )
   {
      vch = d->original ? d->original : d->character;

      if( !vch )
         continue;

      if( !i3_has_name( I3LISTEN( vch ), channel->local_name ) || i3_has_name( I3DENY( vch ), channel->local_name ) )
         continue;

      snprintf( lname, SMST, "%s@%s", CH_I3NAME( vch ), I3_THISMUD );

      if( d->connected == CON_PLAYING && !i3_ignoring( vch, sname ) )
      {
         if( !strcasecmp( lname, tname ) )
         {
            snprintf( buf, LGST, channel->layout_e, channel->local_name, tmsg );
            i3_printf( vch, "%s\r\n", buf );
         }
         else
         {
            snprintf( buf, LGST, channel->layout_e, channel->local_name, omsg );
            i3_printf( vch, "%s\r\n", buf );
         }
      }
   }
   update_chanhistory( channel, omsg );
   return;
}

void I3_process_channel_m( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch = NULL;
   char visname[SMST], buf[LGST];
   I3_CHANNEL *channel;
#ifdef CHAT_TIMESTAMP
   char format[SMST];
   struct tm *local = localtime( &i3_time );
#endif

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      i3_log( "channel_m: received unknown channel (%s)", ps );
      return;
   }

   if( !channel->local_name )
      return;

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( visname, ps, SMST );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

#ifdef CHAT_TIMESTAMP
   strcpy(format, "&G%-2.2d&W:&G%-2.2d&d ");
   strcat(format, channel->layout_m);
   snprintf( buf, LGST, format, local->tm_hour, local->tm_min, channel->local_name, visname, header->originator_mudname, ps );
#else
   snprintf( buf, LGST, channel->layout_m, channel->local_name, visname, header->originator_mudname, ps );
#endif
   for( d = first_descriptor; d; d = d->next )
   {
      vch = d->original ? d->original : d->character;

      if( !vch )
         continue;

      if( !i3_has_name( I3LISTEN( vch ), channel->local_name ) || i3_has_name( I3DENY( vch ), channel->local_name ) )
         continue;

      if( d->connected == CON_PLAYING && !i3_ignoring( vch, visname ) )
         i3_printf( vch, "%s\r\n", buf );
   }
   update_chanhistory( channel, buf );
   return;
}

void I3_process_channel_e( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch = NULL;
   char visname[SMST], msg[LGST], buf[LGST];
   I3_CHANNEL *channel;
#ifdef CHAT_TIMESTAMP
   char format[SMST];
   struct tm *local = localtime( &i3_time );
#endif

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      i3_log( "channel_e: received unknown channel (%s)", ps );
      return;
   }

   if( !channel->local_name )
      return;

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   snprintf( visname, SMST, "%s@%s", ps, header->originator_mudname );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( msg, LGST, "%s", I3_convert_channel_message( ps, visname, visname ) );
#ifdef CHAT_TIMESTAMP
   strcpy(format, "&G%-2.2d&W:&G%-2.2d&d ");
   strcat(format, channel->layout_e);
   snprintf( buf, LGST, format, local->tm_hour, local->tm_min, channel->local_name, msg );
#else
   snprintf( buf, LGST, channel->layout_e, channel->local_name, msg );
#endif

   for( d = first_descriptor; d; d = d->next )
   {
      vch = d->original ? d->original : d->character;

      if( !vch )
         continue;

      if( !i3_has_name( I3LISTEN( vch ), channel->local_name ) || i3_has_name( I3DENY( vch ), channel->local_name ) )
         continue;

      if( d->connected == CON_PLAYING && !i3_ignoring( vch, visname ) )
         i3_printf( vch, "%s\r\n", buf );
   }
   update_chanhistory( channel, buf );
   return;
}

void i3_process_chan_who_req( I3_HEADER * header, char *s )
{
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch;
   char *ps = s, *next_ps;
   char buf[LGST], ibuf[SMST];
   I3_CHANNEL *channel;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( ibuf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      snprintf( buf, LGST, "The channel you specified (%s) is unknown at %s", ps, I3_THISMUD );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-channel", buf );
      i3_log( "chan_who_req: received unknown channel (%s)", ps );
      return;
   }

   if( !channel->local_name )
   {
      snprintf( buf, LGST, "The channel you specified (%s) is not registered at %s", ps, I3_THISMUD );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-channel", buf );
      return;
   }

   i3_write_header( "chan-who-reply", I3_THISMUD, NULL, header->originator_mudname, header->originator_username );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",({" );

   for( d = first_descriptor; d; d = d->next )
   {
      vch = d->original ? d->original : d->character;

      if( !vch )
         continue;

      if( I3ISINVIS( vch ) )
         continue;

      if( i3_has_name( I3LISTEN( vch ), channel->local_name ) && !i3_ignoring( vch, ibuf )
          && !i3_has_name( I3DENY( vch ), channel->local_name ) )
      {
         i3_write_buffer( "\"" );
         i3_write_buffer( CH_I3NAME( vch ) );
         i3_write_buffer( "\"," );
      }
   }
   i3_write_buffer( "}),})\r" );
   i3_send_packet(  );
   return;
}

void I3_process_chan_who_reply( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   CHAR_DATA *ch;

   if( !( ch = i3_find_user( header->target_username ) ) )
   {
      i3_bug( "I3_process_chan_who_reply(): user %s not found.", header->target_username );
      return;
   }

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_printf( ch, "&WUsers listening to %s on %s:\r\n\r\n", ps, header->originator_mudname );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   ps += 2;
   while( 1 )
   {
      if( ps[0] == '}' )
      {
         i3_to_char( "&cNo information returned or no people listening.\r\n", ch );
         return;
      }
      i3_get_field( ps, &next_ps );
      I3_remove_quotes( &ps );
      i3_printf( ch, "&c%s\r\n", ps );

      ps = next_ps;
      if( ps[0] == '}' )
         break;
   }

   return;
}

void I3_send_chan_who( CHAR_DATA * ch, I3_CHANNEL * channel, I3_MUD * mud )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "chan-who-req", I3_THISMUD, CH_I3NAME( ch ), mud->name, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void i3_send_beep( CHAR_DATA * ch, const char *to, I3_MUD * mud )
{
   if( !i3_is_connected(  ) )
      return;

   i3_escape( to );
   i3_write_header( "beep", I3_THISMUD, CH_I3NAME( ch ), mud->name, to );
   i3_write_buffer( "\"" );
   i3_write_buffer( CH_I3NAME( ch ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_beep( I3_HEADER * header, char *s )
{
   char buf[SMST];
   char *ps = s, *next_ps;
   CHAR_DATA *ch;

   snprintf( buf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( ch = i3_find_user( header->target_username ) ) )
   {
      if( !i3exists_player( header->target_username ) )
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      else
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3PERM( ch ) < I3PERM_MORT )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      return;
   }

   if( I3ISINVIS( ch ) || i3_ignoring( ch, buf ) )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_BEEP ) )
   {
      snprintf( buf, SMST, "%s is not accepting beeps.", CH_I3NAME( ch ) );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", buf );
      return;
   }

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   i3_printf( ch, "&Y\a%s@%s i3beeps you.\r\n", ps, header->originator_mudname );
   return;
}

void i3_send_tell( CHAR_DATA * ch, const char *to, I3_MUD * mud, const char *message )
{
   if( !i3_is_connected(  ) )
      return;

   i3_escape( to );
   i3_write_header( "tell", I3_THISMUD, CH_I3NAME( ch ), mud->name, to );
   i3_write_buffer( "\"" );
   i3_write_buffer( CH_I3NAME( ch ) );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( message ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void i3_update_tellhistory( CHAR_DATA * ch, const char *msg )
{
   char new_msg[LGST];
   time_t t = time( NULL );
   struct tm *local = localtime( &t );
   int x;

   snprintf( new_msg, LGST, "&R[%-2.2d:%-2.2d] %s", local->tm_hour, local->tm_min, msg );

   for( x = 0; x < MAX_I3TELLHISTORY; x++ )
   {
      if( I3TELLHISTORY( ch, x ) == '\0' )
      {
         I3TELLHISTORY( ch, x ) = I3STRALLOC( new_msg );
         break;
      }

      if( x == MAX_I3TELLHISTORY - 1 )
      {
         int i;

         for( i = 1; i < MAX_I3TELLHISTORY; i++ )
         {
            I3STRFREE( I3TELLHISTORY( ch, i - 1 ) );
            I3TELLHISTORY( ch, i - 1 ) = I3STRALLOC( I3TELLHISTORY( ch, i ) );
         }
         I3STRFREE( I3TELLHISTORY( ch, x ) );
         I3TELLHISTORY( ch, x ) = I3STRALLOC( new_msg );
      }
   }
   return;
}

void I3_process_tell( I3_HEADER * header, char *s )
{
   char buf[SMST], usr[SMST];
   char *ps = s, *next_ps;
   CHAR_DATA *ch;

   snprintf( buf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( ch = i3_find_user( header->target_username ) ) )
   {
      if( !i3exists_player( header->target_username ) )
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      else
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3PERM( ch ) < I3PERM_MORT )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      return;
   }

   if( I3ISINVIS( ch ) || i3_ignoring( ch, buf ) )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_TELL ) )
   {
      snprintf( buf, SMST, "%s is not accepting tells.", CH_I3NAME( ch ) );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", buf );
      return;
   }

   if( CH_I3AFK( ch ) )
   {
      snprintf( buf, SMST, "%s is currently AFK. Try back later.", CH_I3NAME( ch ) );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", buf );
      return;
   }

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( usr, SMST, "%s@%s", ps, header->originator_mudname );
   snprintf( buf, SMST, "'%s@%s'", ps, header->originator_mudname );

   I3STRFREE( I3REPLY( ch ) );
   I3REPLY( ch ) = I3STRALLOC( buf );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( buf, SMST, "&Y%s i3tells you: &c%s", usr, ps );
   i3_printf( ch, "%s\r\n", buf );
   i3_update_tellhistory( ch, buf );
   return;
}

void i3_send_who( CHAR_DATA * ch, char *mud )
{
   if( !i3_is_connected(  ) )
      return;

   i3_escape( mud );
   i3_write_header( "who-req", I3_THISMUD, CH_I3NAME( ch ), mud, NULL );
   i3_write_buffer( "})\r" );
   i3_send_packet(  );

   return;
}

char *i3_center_line( const char *string, int length )
{
   char stripped[SMST];
   static char outbuf[SMST];
   int amount;

   i3_strlcpy( stripped, i3_strip_colors( string ), SMST );
   amount = length - strlen( stripped );  /* Determine amount to put in front of line */

   if( amount < 1 )
      amount = 1;

   /*
    * Justice, you are the String God! 
    */
   snprintf( outbuf, SMST, "%*s%s%*s", ( amount / 2 ), "", string,
             ( ( amount / 2 ) * 2 ) == amount ? ( amount / 2 ) : ( ( amount / 2 ) + 1 ), "" );

   return outbuf;
}

/*
 * The output of this goes to I3, and thus needs to have its
 * color codes converted to Pinkfish
 */
char *i3_rank_buffer( CHAR_DATA * ch )
{
   static char rbuf[SMST];

   if( I3PERM( ch ) >= I3PERM_IMM )
   {
      i3_strlcpy( rbuf, "&YStaff", SMST );

      if( CH_I3RANK( ch ) && CH_I3RANK( ch )[0] != '\0' )
         snprintf( rbuf, SMST, "&Y%s", CH_I3RANK( ch ) );
   }
   else
   {
      i3_strlcpy( rbuf, "&BPlayer", SMST );

      if( CH_I3RANK( ch ) && CH_I3RANK( ch )[0] != '\0' )
         snprintf( rbuf, SMST, "&B%s", CH_I3RANK( ch ) );
   }
   return rbuf;
}

/* You can customize the output of this - to a point. Because of how the I3 packets are formatted
 * you need to send even the listing header as a person+info packet. It should be fairly obvious
 * how to change this around if you really want to. Use the bogusidle variable for the idle time
 * on a divider, like what I've done here for headerbuf. If you wish to subvert this with your
 * own custom who list, add a #define I3CUSTOMWHO to one of your H files, and make your own
 * i3_process_who_req function somewhere else in your code.
 */
#ifndef I3CUSTOMWHO
void i3_process_who_req( I3_HEADER * header, char *s )
{
   DESCRIPTOR_DATA *d;
   CHAR_DATA *person;
   char ibuf[SMST], personbuf[LGST], tailbuf[LGST];
   char smallbuf[SMST], buf[SMST], outbuf[SMST], stats[SMST], rank[SMST];
   int pcount = 0, xx, yy;
   long int bogusidle = 9999;
#ifdef I3_WILEY
   char boottimebuf[SMST];
#endif

#ifdef I3_WILEY
   strftime( boottimebuf, sizeof(boottimebuf), RFC1123FMT, localtime( (time_t *) &Uptime ) );
#endif
   snprintf( ibuf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   i3_write_header( "who-reply", I3_THISMUD, NULL, header->originator_mudname, header->originator_username );
   i3_write_buffer( "({" );

   i3_write_buffer( "({\"" );
   snprintf( buf, SMST, "&R-=[ &WPlayers on %s &R]=-", I3_THISMUD );
   i3_strlcpy( outbuf, i3_center_line( buf, 78 ), SMST );
   send_to_i3( i3_escape( outbuf ) );

   i3_write_buffer( "\"," );
   snprintf( smallbuf, SMST, "%ld", -1l );
   i3_write_buffer( smallbuf );

   i3_write_buffer( ",\" \",}),({\"" );
   snprintf( buf, SMST, "&Y-=[ &Wtelnet://%s:%d &Y]=-", this_i3mud->telnet, this_i3mud->player_port );
   i3_strlcpy( outbuf, i3_center_line( buf, 78 ), SMST );
   send_to_i3( i3_escape( outbuf ) );

   i3_write_buffer( "\"," );
   snprintf( smallbuf, SMST, "%ld", bogusidle );
   i3_write_buffer( smallbuf );

   i3_write_buffer( ",\" \",})," );

   xx = 0;
   for( d = first_descriptor; d; d = d->next )
   {
      person = d->original ? d->original : d->character;

      if( person && d->connected >= CON_PLAYING )
      {
         if( I3PERM( person ) < I3PERM_MORT || I3PERM( person ) >= I3PERM_IMM
             || I3ISINVIS( person ) || i3_ignoring( person, ibuf ) )
            continue;

         pcount++;

         if( xx == 0 )
         {
            i3_write_buffer( "({\"" );
            send_to_i3( i3_escape
                        ( "&B--------------------------------=[ &WPlayers &B]=---------------------------------" ) );
            i3_write_buffer( "\"," );
            snprintf( smallbuf, SMST, "%ld", bogusidle );
            i3_write_buffer( smallbuf );
            i3_write_buffer( ",\" \",})," );
         }

         i3_write_buffer( "({\"" );

         i3_strlcpy( rank, i3_rank_buffer( person ), SMST );
         i3_strlcpy( outbuf, i3_center_line( rank, 20 ), SMST );
         send_to_i3( i3_escape( outbuf ) );

         i3_write_buffer( "\"," );
         snprintf( smallbuf, SMST, "%ld", -1l );
         i3_write_buffer( smallbuf );
         i3_write_buffer( ",\"" );

         i3_strlcpy( stats, "&z[", SMST );
         if( CH_I3AFK( person ) )
            i3_strlcat( stats, "AFK", SMST );
         else
            i3_strlcat( stats, "---", SMST );
         i3_strlcat( stats, "]&G", SMST );

         snprintf( personbuf, LGST, "%s %s%s", stats, CH_I3NAME( person ), CH_I3TITLE( person ) );
         send_to_i3( i3_escape( personbuf ) );
         i3_write_buffer( "\",})," );
         xx++;
      }
   }

   yy = 0;
   for( d = first_descriptor; d; d = d->next )
   {
      person = d->original ? d->original : d->character;

      if( person && d->connected >= CON_PLAYING )
      {
         if( I3PERM( person ) < I3PERM_IMM || I3ISINVIS( person ) || i3_ignoring( person, ibuf ) )
            continue;

         pcount++;

         if( yy == 0 )
         {
            i3_write_buffer( "({\"" );
            send_to_i3( i3_escape
                        ( "&R-------------------------------=[ &WImmortals &R]=--------------------------------" ) );
            i3_write_buffer( "\"," );
            if( xx > 0 )
               snprintf( smallbuf, SMST, "%ld", bogusidle * 3 );
            else
               snprintf( smallbuf, SMST, "%ld", bogusidle );
            i3_write_buffer( smallbuf );
            i3_write_buffer( ",\" \",})," );
         }
         i3_write_buffer( "({\"" );

         i3_strlcpy( rank, i3_rank_buffer( person ), SMST );
         i3_strlcpy( outbuf, i3_center_line( rank, 20 ), SMST );
         send_to_i3( i3_escape( outbuf ) );

         i3_write_buffer( "\"," );
         snprintf( smallbuf, SMST, "%ld", -1l );
         i3_write_buffer( smallbuf );
         i3_write_buffer( ",\"" );

         i3_strlcpy( stats, "&z[", SMST );
         if( CH_I3AFK( person ) )
            i3_strlcat( stats, "AFK", SMST );
         else
            i3_strlcat( stats, "---", SMST );
         i3_strlcat( stats, "]&G", SMST );

         snprintf( personbuf, LGST, "%s %s%s", stats, CH_I3NAME( person ), CH_I3TITLE( person ) );
         send_to_i3( i3_escape( personbuf ) );
         i3_write_buffer( "\",})," );
         yy++;
      }
   }

   i3_write_buffer( "({\"" );
   snprintf( tailbuf, LGST, "&Y[&W%d Player%s&Y]", pcount, pcount == 1 ? "" : "s" );
   send_to_i3( i3_escape( tailbuf ) );
   i3_write_buffer( "\"," );
   snprintf( smallbuf, SMST, "%ld", bogusidle * 2 );
   i3_write_buffer( smallbuf );
   i3_write_buffer( ",\"" );
#ifdef I3_WILEY
   snprintf( tailbuf, LGST, "&Y[&WHomepage: %s&Y] [&W%d Max Since Reboot&Y]", HTTP_PLACEHOLDER, MAXPLAYERS_PLACEHOLDER );
   send_to_i3( i3_escape( tailbuf ) );
   i3_write_buffer( "\",})," );

   i3_write_buffer( "({\"" );
   snprintf( tailbuf, LGST, "&Y[&W%d logins since last reboot on %s&Y]", NUMLOGINS_PLACEHOLDER, boottimebuf );
   send_to_i3( i3_escape( tailbuf ) );
   i3_write_buffer( "\"," );
   snprintf( smallbuf, SMST, "%ld", bogusidle );
   i3_write_buffer( smallbuf );
   i3_write_buffer( ",\" \",}),}),})\r" );
#else
   snprintf( tailbuf, LGST, "&Y[&WHomepage: %s&Y]", this_i3mud->web );
   send_to_i3( i3_escape( tailbuf ) );
   i3_write_buffer( "\",}),}),})\r" );
#endif
   i3_send_packet(  );

   return;
}
#endif

/* This is where the incoming results of a who-reply packet are processed.
 * Note that rather than just spit the names out, I've copied the packet fields into
 * buffers to be output later. Also note that if it receives an idle value of 9999
 * the normal 30 space output will be bypassed. This is so that muds who want to
 * customize the listing headers in their who-reply packets can do so and the results
 * won't get chopped off after the 30th character. If for some reason a person on
 * the target mud just happens to have been idling for 9999 cycles, their data may
 * be displayed strangely compared to the rest. But I don't expect that 9999 is a very
 * common length of time to be idle either :P
 * Receving an idle value of 19998 may also cause odd results since this is used
 * to indicate receipt of the last line of a who, which is typically the number of
 * visible players found.
 */
void I3_process_who_reply( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps, *next_ps2;
   CHAR_DATA *ch;
   char person[LGST], title[SMST];
   int idle;

   if( !( ch = i3_find_user( header->target_username ) ) )
      return;

   ps += 2;

   while( 1 )
   {
      if( ps[0] == '}' )
      {
         i3_to_char( "&WNo information returned.\r\n", ch );
         return;
      }

      i3_get_field( ps, &next_ps );

      ps += 2;
      i3_get_field( ps, &next_ps2 );
      I3_remove_quotes( &ps );
      i3_strlcpy( person, ps, LGST );
      ps = next_ps2;
      i3_get_field( ps, &next_ps2 );
      idle = atoi( ps );
      ps = next_ps2;
      i3_get_field( ps, &next_ps2 );
      I3_remove_quotes( &ps );
      i3_strlcpy( title, ps, SMST );
      ps = next_ps2;

      if( idle == 9999 )
         i3_printf( ch, "%s %s\r\n\r\n", person, title );
      else if( idle == 19998 )
         i3_printf( ch, "\r\n%s %s\r\n", person, title );
      else if( idle == 29997 )
         i3_printf( ch, "\r\n%s %s\r\n\r\n", person, title );
      else
         i3_printf( ch, "%s %s\r\n", person, title );

      ps = next_ps;
      if( ps[0] == '}' )
         break;
   }
   return;
}

void i3_send_emote_to( CHAR_DATA * ch, const char *to, I3_MUD * mud, const char *message )
{
   char buf[LGST];

   if( !i3_is_connected(  ) )
      return;

   if( strstr( message, "$N" ) == NULL )
      snprintf( buf, LGST, "$N %s", message );
   else
      i3_strlcpy( buf, message, LGST );

   i3_escape( to );
   i3_write_header( "emoteto", I3_THISMUD, CH_I3NAME( ch ), mud->name, to );
   i3_write_buffer( "\"" );
   i3_write_buffer( CH_I3NAME( ch ) );
   i3_write_buffer( "\",\"" );
   send_to_i3( i3_escape( buf ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_emoteto( I3_HEADER * header, char *s )
{
   CHAR_DATA *ch;
   char *ps = s, *next_ps;
   char visname[SMST], buf[SMST];
   char msg[LGST];

   snprintf( buf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( ch = i3_find_user( header->target_username ) ) )
   {
      if( !i3exists_player( header->target_username ) )
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      else
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3PERM( ch ) < I3PERM_MORT )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      return;
   }

   if( I3ISINVIS( ch ) || i3_ignoring( ch, buf ) || !ch->desc )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   snprintf( visname, SMST, "%s@%s", ps, header->originator_mudname );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( msg, LGST, "&c%s\r\n", I3_convert_channel_message( ps, visname, visname ) );
   i3_to_char( msg, ch );
   return;
}

void I3_send_finger( CHAR_DATA * ch, char *user, char *mud )
{
   if( !i3_is_connected(  ) )
      return;

   i3_escape( mud );

   i3_write_header( "finger-req", I3_THISMUD, CH_I3NAME( ch ), mud, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( i3_escape( user ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

/* The output of this was slightly modified to resemble the Finger snippet */
void I3_process_finger_reply( I3_HEADER * header, char *s )
{
   CHAR_DATA *ch;
   char *ps = s, *next_ps;
   char title[SMST], email[SMST], last[SMST], level[SMST];

   if( !( ch = i3_find_user( header->target_username ) ) )
      return;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_printf( ch, "&wI3FINGER information for &G%s@%s\r\n", ps, header->originator_mudname );
   i3_to_char( "&w-------------------------------------------------\r\n", ch );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( title, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( email, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( last, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( level, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   i3_printf( ch, "&wTitle: &G%s\r\n", title );
   i3_printf( ch, "&wLevel: &G%s\r\n", level );
   i3_printf( ch, "&wEmail: &G%s\r\n", email );
   i3_printf( ch, "&wHTTP : &G%s\r\n", ps );
   i3_printf( ch, "&wLast on: &G%s\r\n", last );

   return;
}

void I3_process_finger_req( I3_HEADER * header, char *s )
{
   CHAR_DATA *ch;
   char *ps = s, *next_ps;
   char smallbuf[200], buf[SMST];

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( buf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( ch = i3_find_user( ps ) ) )
   {
      if( !i3exists_player( ps ) )
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      else
         I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3PERM( ch ) < I3PERM_MORT )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "No such player." );
      return;
   }

   if( I3ISINVIS( ch ) || i3_ignoring( ch, buf ) )
   {
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", "That player is offline." );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_DENYFINGER ) || I3IS_SET( I3FLAG( ch ), I3_PRIVACY ) )
   {
      snprintf( buf, SMST, "%s is not accepting fingers.", CH_I3NAME( ch ) );
      I3_send_error( header->originator_mudname, header->originator_username, "unk-user", buf );
      return;
   }

   i3_printf( ch, "%s@%s has requested your i3finger information.\r\n",
              header->originator_username, header->originator_mudname );

   i3_write_header( "finger-reply", I3_THISMUD, NULL, header->originator_mudname, header->originator_username );
   i3_write_buffer( "\"" );
   i3_write_buffer( i3_escape( CH_I3NAME( ch ) ) );
   i3_write_buffer( "\",\"" );
   i3_write_buffer( i3_escape( CH_I3NAME( ch ) ) );
   send_to_i3( i3_escape( CH_I3TITLE( ch ) ) );
   i3_write_buffer( "\",\"\",\"" );
#ifdef FINGERCODE
   if( ch->pcdata->email )
   {
      if( !IS_SET( ch->pcdata->flags, PCFLAG_PRIVACY ) )
         i3_write_buffer( ch->pcdata->email );
      else
         i3_write_buffer( "[Private]" );
   }
#else
   i3_write_buffer( "Not supported" );
#endif
   i3_write_buffer( "\",\"" );
   i3_strlcpy( smallbuf, "-1", 200 );   /* online since */
   i3_write_buffer( smallbuf );
   i3_write_buffer( "\"," );
   snprintf( smallbuf, 200, "%ld", -1l );
   i3_write_buffer( smallbuf );
   i3_write_buffer( ",\"" );
   i3_write_buffer( "[PRIVATE]" );
   i3_write_buffer( "\",\"" );
   snprintf( buf, SMST, "%s", i3_rank_buffer( ch ) );
   send_to_i3( buf );
#ifdef FINGERCODE
   i3_write_buffer( "\",\"" );
   if( ch->pcdata->homepage )
      i3_write_buffer( i3_escape( ch->pcdata->homepage ) );
   else
      i3_write_buffer( "Not Provided" );
   i3_write_buffer( "\",})\r" );
#else
   i3_write_buffer( "\",\"Not Suported\",})\r" );
#endif
   i3_send_packet(  );

   return;
}

void i3_send_locate( CHAR_DATA * ch, const char *user )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "locate-req", I3_THISMUD, CH_I3NAME( ch ), NULL, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( i3_escape( user ) );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_locate_reply( I3_HEADER * header, char *s )
{
   char mud_name[SMST], user_name[SMST], status[SMST];
   char *ps = s, *next_ps;
   CHAR_DATA *ch;

   if( !( ch = i3_find_user( header->target_username ) ) )
      return;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( mud_name, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( user_name, ps, SMST );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   ps = next_ps;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( status, ps, SMST );

   if( !strcasecmp( status, "active" ) )
      i3_strlcpy( status, "Online", SMST );

   if( !strcasecmp( status, "exists, but not logged on" ) )
      i3_strlcpy( status, "Offline", SMST );

   i3_printf( ch, "&RI3 Locate: &Y%s@%s: &c%s.\r\n", user_name, mud_name, status );
   return;
}

void I3_process_locate_req( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   char smallbuf[50], buf[SMST];
   CHAR_DATA *ch;
   bool choffline = FALSE;

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );

   snprintf( buf, SMST, "%s@%s", header->originator_username, header->originator_mudname );

   if( !( ch = i3_find_user( ps ) ) )
   {
      if( i3exists_player( ps ) )
         choffline = TRUE;
      else
         return;
   }

   if( ch )
   {
      if( I3PERM( ch ) < I3PERM_MORT )
         return;

      if( I3ISINVIS( ch ) )
         choffline = TRUE;

      if( i3_ignoring( ch, buf ) )
         choffline = TRUE;
   }

   i3_write_header( "locate-reply", I3_THISMUD, NULL, header->originator_mudname, header->originator_username );
   i3_write_buffer( "\"" );
   i3_write_buffer( I3_THISMUD );
   i3_write_buffer( "\",\"" );
   if( !choffline )
      i3_write_buffer( CH_I3NAME( ch ) );
   else
      i3_write_buffer( i3_capitalize( ps ) );
   i3_write_buffer( "\"," );
   snprintf( smallbuf, 50, "%ld", -1l );
   i3_write_buffer( smallbuf );
   if( !choffline )
      i3_write_buffer( ",\"Online\",})\r" );
   else
      i3_write_buffer( ",\"Offline\",})\r" );
   i3_send_packet(  );

   return;
}

void i3_send_channel_listen( I3_CHANNEL * channel, bool lconnect )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-listen", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\"," );
   if( lconnect )
      i3_write_buffer( "1,})\r" );
   else
      i3_write_buffer( "0,})\r" );
   i3_send_packet(  );

   return;
}

void I3_process_channel_adminlist_reply( I3_HEADER * header, char *s )
{
   char *ps = s, *next_ps;
   I3_CHANNEL *channel;
   CHAR_DATA *ch;

   if( ( ch = i3_find_user( header->target_username ) ) == NULL )
   {
      i3_bug( "I3_process_channel_adminlist_reply(): user %s not found.", header->target_username );
      return;
   }

   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   if( !( channel = find_I3_channel_by_name( ps ) ) )
   {
      i3_bug( "I3_process_channel_adminlist_reply(): Invalid local channel %s reply received.", ps );
      return;
   }
   i3_printf( ch, "&RThe following muds are %s %s:\r\n\r\n", channel->status == 0 ? "banned from" : "invited to",
              channel->local_name );

   ps = next_ps;
   i3_get_field( ps, &next_ps );
   ps += 2;
   while( 1 )
   {
      if( ps[0] == '}' )
      {
         i3_to_char( "&YNo entries found.\r\n", ch );
         return;
      }

      i3_get_field( ps, &next_ps );
      I3_remove_quotes( &ps );
      i3_printf( ch, "&Y%s\r\n", ps );

      ps = next_ps;
      if( ps[0] == '}' )
         break;
   }
   return;
}

void I3_send_channel_adminlist( CHAR_DATA * ch, char *chan_name )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "chan-adminlist", I3_THISMUD, CH_I3NAME( ch ), I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( chan_name );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );

   return;
}

void I3_send_channel_admin( CHAR_DATA * ch, char *chan_name, char *list )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-admin", I3_THISMUD, CH_I3NAME( ch ), I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( chan_name );
   i3_write_buffer( "\"," );
   i3_write_buffer( list );
   i3_write_buffer( "})\r" );
   i3_send_packet(  );

   return;
}

void I3_send_channel_add( CHAR_DATA * ch, char *arg, int type )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-add", I3_THISMUD, CH_I3NAME( ch ), I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( arg );
   i3_write_buffer( "\"," );
   switch ( type )
   {
      default:
         i3_bug( "%s", "I3_send_channel_add: Illegal channel type!" );
         return;
      case 0:
         i3_write_buffer( "0,})\r" );
         break;
      case 1:
         i3_write_buffer( "1,})\r" );
         break;
      case 2:
         i3_write_buffer( "2,})\r" );
         break;
   }
   i3_send_packet(  );
   return;
}

void I3_send_channel_remove( CHAR_DATA * ch, I3_CHANNEL * channel )
{
   if( !i3_is_connected(  ) )
      return;

   i3_write_header( "channel-remove", I3_THISMUD, CH_I3NAME( ch ), I3_ROUTER_NAME, NULL );
   i3_write_buffer( "\"" );
   i3_write_buffer( channel->I3_name );
   i3_write_buffer( "\",})\r" );
   i3_send_packet(  );
   return;
}

void i3_send_shutdown( int delay )
{
   I3_CHANNEL *channel;
   char s[50];

   if( !i3_is_connected(  ) )
      return;

   for( channel = first_I3chan; channel; channel = channel->next )
   {
      if( channel->local_name && channel->local_name[0] != '\0' )
         i3_send_channel_listen( channel, FALSE );
   }

   i3_write_header( "shutdown", I3_THISMUD, NULL, I3_ROUTER_NAME, NULL );
   snprintf( s, 50, "%d", delay );
   i3_write_buffer( s );
   i3_write_buffer( ",})\r" );

   if( !i3_write_packet( I3_output_buffer ) )
      i3_close_connection( FALSE );

   return;
}

/*
 * Read the header of an I3 packet. pps will point to the next field
 * of the packet.
 */
I3_HEADER *i3_get_header( char **pps )
{
   I3_HEADER *header;
   char *ps = *pps, *next_ps;

   I3CREATE( header, I3_HEADER, 1 );

   i3_get_field( ps, &next_ps );
   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( header->originator_mudname, ps, SMST );
   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( header->originator_username, ps, SMST );
   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( header->target_mudname, ps, SMST );
   ps = next_ps;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( header->target_username, ps, SMST );

   *pps = next_ps;
   return header;
}

/*
 * Read the first field of an I3 packet and call the proper function to
 * process it. Afterwards the original I3 packet is completly messed up.
 *
 * Reworked on 9-5-03 by Samson to be made more efficient with regard to banned muds.
 * Also only need to gather the header information once this way instead of in multiple places.
 */
void I3_parse_packet( void )
{
   I3_HEADER *header = NULL;
   char *ps, *next_ps;
   char ptype[SMST];

   ps = I3_currentpacket;
   if( ps[0] != '(' || ps[1] != '{' )
      return;

   if( packetdebug )
      i3_log( "Packet received: %s", ps );

   ps += 2;
   i3_get_field( ps, &next_ps );
   I3_remove_quotes( &ps );
   i3_strlcpy( ptype, ps, SMST );

   header = i3_get_header( &ps );

   /*
    * There. Nice and simple, no? 
    */
   if( i3_banned( header->originator_mudname ) )
      return;

   if( !strcasecmp( ptype, "tell" ) )
      I3_process_tell( header, ps );

   if( !strcasecmp( ptype, "beep" ) )
      I3_process_beep( header, ps );

   if( !strcasecmp( ptype, "emoteto" ) )
      I3_process_emoteto( header, ps );

   if( !strcasecmp( ptype, "channel-m" ) )
      I3_process_channel_m( header, ps );

   if( !strcasecmp( ptype, "channel-e" ) )
      I3_process_channel_e( header, ps );

   if( !strcasecmp( ptype, "chan-filter-req" ) )
      I3_process_channel_filter( header, ps );

   if( !strcasecmp( ptype, "finger-req" ) )
      I3_process_finger_req( header, ps );

   if( !strcasecmp( ptype, "finger-reply" ) )
      I3_process_finger_reply( header, ps );

   if( !strcasecmp( ptype, "locate-req" ) )
      I3_process_locate_req( header, ps );

   if( !strcasecmp( ptype, "locate-reply" ) )
      I3_process_locate_reply( header, ps );

   if( !strcasecmp( ptype, "chan-who-req" ) )
      i3_process_chan_who_req( header, ps );

   if( !strcasecmp( ptype, "chan-who-reply" ) )
      I3_process_chan_who_reply( header, ps );

   if( !strcasecmp( ptype, "chan-adminlist-reply" ) )
      I3_process_channel_adminlist_reply( header, ps );

   if( !strcasecmp( ptype, "ucache-update" ) && this_i3mud->ucache == TRUE )
      i3_process_ucache_update( header, ps );

   if( !strcasecmp( ptype, "who-req" ) )
      i3_process_who_req( header, ps );

   if( !strcasecmp( ptype, "who-reply" ) )
      I3_process_who_reply( header, ps );

   if( !strcasecmp( ptype, "chanlist-reply" ) )
      i3_process_chanlist_reply( header, ps );

   if( !strcasecmp( ptype, "startup-reply" ) )
      I3_process_startup_reply( header, ps );

   if( !strcasecmp( ptype, "mudlist" ) )
      I3_process_mudlist( header, ps );

   if( !strcasecmp( ptype, "error" ) )
      I3_process_error( header, ps );

   if( !strcasecmp( ptype, "chan-ack" ) )
      I3_process_chanack( header, ps );

   if( !strcasecmp( ptype, "channel-t" ) )
      I3_process_channel_t( header, ps );

   if( !strcasecmp( ptype, "chan-user-req" ) )
      I3_process_chan_user_req( header, ps );

   if( !strcasecmp( ptype, "chan-user-reply" ) && this_i3mud->ucache == TRUE )
      I3_process_chan_user_reply( header, ps );

   if( !strcasecmp( ptype, "router-shutdown" ) )
   {
      int delay;

      i3_get_field( ps, &next_ps );
      delay = atoi( ps );

      if( delay == 0 )
      {
         i3_log( "Router %s is shutting down.", I3_ROUTER_NAME );
         i3_close_connection( FALSE );
      }
      else
      {
         i3_log( "Router %s is rebooting and will be back in %d second%s.", I3_ROUTER_NAME, delay, delay == 1 ? "" : "s" );
         i3_close_connection( TRUE );
      }
   }
   I3DISPOSE( header );
   return;
}

/*
 * Read one I3 packet into the I3_input_buffer
 */
void I3_read_packet( void )
{
   long size;

   memmove( &size, I3_input_buffer, 4 );
   size = ntohl( size );

   memmove( I3_currentpacket, I3_input_buffer + 4, size );

   if( I3_currentpacket[size - 1] != ')' )
      I3_currentpacket[size - 1] = 0;
   I3_currentpacket[size] = 0;

   memmove( I3_input_buffer, I3_input_buffer + size + 4, I3_input_pointer - size - 4 );
   I3_input_pointer -= size + 4;
   return;
}

/************************************
 * User login and logout functions. *
 ************************************/

void i3_init_char( CHAR_DATA * ch )
{
   if( IS_NPC( ch ) )
      return;

   I3CREATE( CH_I3DATA( ch ), I3_CHARDATA, 1 );
   I3LISTEN( ch ) = NULL;
   I3DENY( ch ) = NULL;
   I3REPLY( ch ) = NULL;
   I3FLAG( ch ) = 0;
   I3SET_BIT( I3FLAG( ch ), I3_COLORFLAG );  /* Default color to on. People can turn this off if they hate it. */
   FIRST_I3IGNORE( ch ) = NULL;
   LAST_I3IGNORE( ch ) = NULL;
   I3PERM( ch ) = I3PERM_NOTSET;

   return;
}

void i3_free_chardata( CHAR_DATA * ch )
{
   I3_IGNORE *temp, *next;
   int x;

   if( IS_NPC( ch ) )
      return;

   if( !CH_I3DATA( ch ) )
      return;

   I3STRFREE( I3LISTEN( ch ) );
   I3STRFREE( I3DENY( ch ) );
   I3STRFREE( I3REPLY( ch ) );

   if( FIRST_I3IGNORE( ch ) )
   {
      for( temp = FIRST_I3IGNORE( ch ); temp; temp = next )
      {
         next = temp->next;
         I3STRFREE( temp->name );
         I3UNLINK( temp, FIRST_I3IGNORE( ch ), LAST_I3IGNORE( ch ), next, prev );
         I3DISPOSE( temp );
      }
   }
   for( x = 0; x < MAX_I3TELLHISTORY; x++ )
      I3STRFREE( I3TELLHISTORY( ch, x ) );

   I3DISPOSE( CH_I3DATA( ch ) );
   return;
}

void i3_adjust_perms( CHAR_DATA * ch )
{
   if( !this_i3mud )
      return;

   /*
    * Ugly hack to let the permission system adapt freely, but retains the ability to override that adaptation
    * * in the event you need to restrict someone to a lower level, or grant someone a higher level. This of
    * * course comes at the cost of forgetting you may have done so and caused the override flag to be set, but hey.
    * * This isn't a perfect system and never will be. Samson 2-8-04.
    */
   if( !I3IS_SET( I3FLAG( ch ), I3_PERMOVERRIDE ) )
   {
      if( CH_I3LEVEL( ch ) < this_i3mud->minlevel )
         I3PERM( ch ) = I3PERM_NONE;
      else if( CH_I3LEVEL( ch ) >= this_i3mud->minlevel && CH_I3LEVEL( ch ) < this_i3mud->immlevel )
         I3PERM( ch ) = I3PERM_MORT;
      else if( CH_I3LEVEL( ch ) >= this_i3mud->immlevel && CH_I3LEVEL( ch ) < this_i3mud->adminlevel )
         I3PERM( ch ) = I3PERM_IMM;
      else if( CH_I3LEVEL( ch ) >= this_i3mud->adminlevel && CH_I3LEVEL( ch ) < this_i3mud->implevel )
         I3PERM( ch ) = I3PERM_ADMIN;
      else if( CH_I3LEVEL( ch ) >= this_i3mud->implevel )
         I3PERM( ch ) = I3PERM_IMP;
   }
   return;
}

void i3_char_login( CHAR_DATA * ch )
{
   int gender, sex;
   char buf[SMST];

   if( !this_i3mud )
      return;

   i3_adjust_perms( ch );

   if( !i3_is_connected(  ) )
   {
      if( I3PERM( ch ) >= I3PERM_IMM && i3wait == -2 )
         i3_to_char
            ( "&RThe Intermud-3 connection is down. Attempts to reconnect were abandoned due to excessive failures.\r\n",
              ch );
      return;
   }

   if( I3PERM( ch ) < I3PERM_MORT )
      return;

   if( this_i3mud->ucache == TRUE )
   {
      snprintf( buf, SMST, "%s@%s", CH_I3NAME( ch ), I3_THISMUD );
      gender = I3_get_ucache_gender( buf );
      sex = dikutoi3gender( CH_I3SEX( ch ) );

      if( gender == sex )
         return;

      I3_ucache_update( buf, sex );
      if( !I3IS_SET( I3FLAG( ch ), I3_INVIS ) )
         i3_send_ucache_update( CH_I3NAME( ch ), sex );
   }
   return;
}

bool i3_load_char( CHAR_DATA * ch, FILE * fp, const char *word )
{
   bool fMatch = FALSE;

   if( IS_NPC( ch ) )
      return FALSE;

   if( I3PERM( ch ) == I3PERM_NOTSET )
      i3_adjust_perms( ch );

   switch ( UPPER( word[0] ) )
   {
      case 'I':
         I3KEY( "i3perm", I3PERM( ch ), i3_fread_number( fp ) );
         if( !strcasecmp( word, "i3flags" ) )
         {
            I3FLAG( ch ) = i3_fread_number( fp );
            i3_char_login( ch );
            fMatch = TRUE;
            break;
         }

         if( !strcasecmp( word, "i3listen" ) )
         {
            I3LISTEN( ch ) = i3_fread_line( fp );
            if( I3LISTEN( ch ) != NULL && i3_is_connected(  ) )
            {
               I3_CHANNEL *channel = NULL;
               const char *channels = I3LISTEN( ch );
               char arg[SMST];

               while( 1 )
               {
                  if( channels[0] == '\0' )
                     break;
                  channels = i3_one_argument( channels, arg );

                  if( !( channel = find_I3_channel_by_localname( arg ) ) )
                     i3_unflagchan( &I3LISTEN( ch ), arg );
                  if( channel && I3PERM( ch ) < channel->i3perm )
                     i3_unflagchan( &I3LISTEN( ch ), arg );
               }
            }
            fMatch = TRUE;
            break;
         }

         if( !strcasecmp( word, "i3deny" ) )
         {
            I3DENY( ch ) = i3_fread_line( fp );
            if( I3DENY( ch ) != NULL && i3_is_connected(  ) )
            {
               I3_CHANNEL *channel = NULL;
               const char *channels = I3DENY( ch );
               char arg[SMST];

               while( 1 )
               {
                  if( channels[0] == '\0' )
                     break;
                  channels = i3_one_argument( channels, arg );

                  if( !( channel = find_I3_channel_by_localname( arg ) ) )
                     i3_unflagchan( &I3DENY( ch ), arg );
                  if( channel && I3PERM( ch ) < channel->i3perm )
                     i3_unflagchan( &I3DENY( ch ), arg );
               }
            }
            fMatch = TRUE;
            break;
         }
         if( !strcasecmp( word, "i3ignore" ) )
         {
            I3_IGNORE *temp;

            I3CREATE( temp, I3_IGNORE, 1 );
            temp->name = i3_fread_line( fp );
            I3LINK( temp, FIRST_I3IGNORE( ch ), LAST_I3IGNORE( ch ), next, prev );
            fMatch = TRUE;
            break;
         }
         break;
   }
   return fMatch;
}

void i3_save_char( CHAR_DATA * ch, FILE * fp )
{
   I3_IGNORE *temp;

   if( IS_NPC( ch ) )
      return;

   fprintf( fp, "i3perm       %d\n", I3PERM( ch ) );
   fprintf( fp, "i3flags      %d\n", I3FLAG( ch ) );
   if( I3LISTEN( ch ) && I3LISTEN( ch )[0] != '\0' )
      fprintf( fp, "i3listen     %s\n", I3LISTEN( ch ) );
   if( I3DENY( ch ) && I3DENY( ch )[0] != '\0' )
      fprintf( fp, "i3deny       %s\n", I3DENY( ch ) );
   for( temp = FIRST_I3IGNORE( ch ); temp; temp = temp->next )
      fprintf( fp, "i3ignore     %s\n", temp->name );
   return;
}

/*******************************************
 * Network Startup and Shutdown functions. *
 *******************************************/

void i3_savecolor( void )
{
   FILE *fp;
   I3_COLOR *color;

   if( ( fp = fopen( I3_COLOR_FILE, "w" ) ) == NULL )
   {
      i3_log( "%s", "Couldn't write to I3 color file." );
      return;
   }

   for( color = first_i3_color; color; color = color->next )
   {
      fprintf( fp, "%s", "#COLOR\n" );
      fprintf( fp, "Name   %s\n", color->name );
      fprintf( fp, "Mudtag %s\n", color->mudtag );
      fprintf( fp, "I3tag  %s\n", color->i3tag );
      fprintf( fp, "I3fish %s\n", color->i3fish );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

void i3_readcolor( I3_COLOR * color, FILE * fp )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : i3_fread_word( fp );
      fMatch = FALSE;

      switch ( word[0] )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fp );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;

         case 'I':
            I3KEY( "I3tag", color->i3tag, i3_fread_line( fp ) );
            I3KEY( "I3fish", color->i3fish, i3_fread_line( fp ) );
            break;

         case 'M':
            I3KEY( "Mudtag", color->mudtag, i3_fread_line( fp ) );
            break;

         case 'N':
            I3KEY( "Name", color->name, i3_fread_line( fp ) );
            break;
      }
      if( !fMatch )
         i3_bug( "i3_readcolor: no match: %s", word );
   }
}

void i3_load_color_table( void )
{
   FILE *fp;
   I3_COLOR *color;

   first_i3_color = last_i3_color = NULL;

   i3_log( "%s", "Loading color table..." );

   if( !( fp = fopen( I3_COLOR_FILE, "r" ) ) )
   {
      i3_log( "%s", "No color table found." );
      return;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fp );
      if( letter == '*' )
      {
         i3fread_to_eol( fp );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_color_table: # not found." );
         break;
      }

      word = i3_fread_word( fp );
      if( !strcasecmp( word, "COLOR" ) )
      {
         I3CREATE( color, I3_COLOR, 1 );
         i3_readcolor( color, fp );
         I3LINK( color, first_i3_color, last_i3_color, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_color_table: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fp );
   return;
}

void i3_savehelps( void )
{
   FILE *fp;
   I3_HELP_DATA *help;

   if( ( fp = fopen( I3_HELP_FILE, "w" ) ) == NULL )
   {
      i3_log( "%s", "Couldn't write to I3 help file." );
      return;
   }

   for( help = first_i3_help; help; help = help->next )
   {
      fprintf( fp, "%s", "#HELP\n" );
      fprintf( fp, "Name %s\n", help->name );
      fprintf( fp, "Perm %s\n", perm_names[help->level] );
      fprintf( fp, "Text %s¢\n", help->text );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

void I3_readhelp( I3_HELP_DATA * help, FILE * fp )
{
   const char *word;
   char hbuf[LGST];
   int permvalue;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : i3_fread_word( fp );
      fMatch = FALSE;

      switch ( word[0] )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fp );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;

         case 'N':
            I3KEY( "Name", help->name, i3_fread_line( fp ) );
            break;

         case 'P':
            if( !strcasecmp( word, "Perm" ) )
            {
               word = i3_fread_word( fp );
               permvalue = get_permvalue( word );

               if( permvalue < 0 || permvalue > I3PERM_IMP )
               {
                  i3_bug( "i3_readhelp: Command %s loaded with invalid permission. Set to Imp.", help->name );
                  help->level = I3PERM_IMP;
               }
               else
                  help->level = permvalue;
               fMatch = TRUE;
               break;
            }
            break;

         case 'T':
            if( !strcasecmp( word, "Text" ) )
            {
               int num = 0;

               while( ( hbuf[num] = fgetc( fp ) ) != EOF && hbuf[num] != '¢' && num < ( LGST - 2 ) )
                  num++;
               hbuf[num] = '\0';
               help->text = I3STRALLOC( hbuf );
               fMatch = TRUE;
               break;
            }
            I3KEY( "Text", help->text, i3_fread_line( fp ) );
            break;
      }
      if( !fMatch )
         i3_bug( "i3_readhelp: no match: %s", word );
   }
}

void I3_load_helps( void )
{
   FILE *fp;
   I3_HELP_DATA *help;

   first_i3_help = last_i3_help = NULL;

   i3_log( "%s", "Loading I3 help file..." );

   if( !( fp = fopen( I3_HELP_FILE, "r" ) ) )
   {
      i3_log( "%s", "No help file found." );
      return;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fp );
      if( letter == '*' )
      {
         i3fread_to_eol( fp );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_helps: # not found." );
         break;
      }

      word = i3_fread_word( fp );
      if( !strcasecmp( word, "HELP" ) )
      {
         I3CREATE( help, I3_HELP_DATA, 1 );
         I3_readhelp( help, fp );
         I3LINK( help, first_i3_help, last_i3_help, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_helps: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fp );
   return;
}

void I3_savecommands( void )
{
   FILE *fp;
   I3_CMD_DATA *cmd;
   I3_ALIAS *alias;

   if( !( fp = fopen( I3_CMD_FILE, "w" ) ) )
   {
      i3_log( "%s", "Couldn't write to I3 command file." );
      return;
   }

   for( cmd = first_i3_command; cmd; cmd = cmd->next )
   {
      fprintf( fp, "%s", "#COMMAND\n" );
      fprintf( fp, "Name      %s\n", cmd->name );
      if( cmd->function != NULL )
         fprintf( fp, "Code      %s\n", i3_funcname( cmd->function ) );
      else
         fprintf( fp, "%s", "Code      NULL\n" );
      fprintf( fp, "Perm      %s\n", perm_names[cmd->level] );
      fprintf( fp, "Connected %d\n", cmd->connected );
      for( alias = cmd->first_alias; alias; alias = alias->next )
         fprintf( fp, "Alias     %s\n", alias->name );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

void I3_readcommand( I3_CMD_DATA * cmd, FILE * fp )
{
   I3_ALIAS *alias;
   const char *word;
   int permvalue;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : i3_fread_word( fp );
      fMatch = FALSE;

      switch ( word[0] )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fp );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;

         case 'A':
            if( !strcasecmp( word, "Alias" ) )
            {
               I3CREATE( alias, I3_ALIAS, 1 );
               alias->name = i3_fread_line( fp );
               I3LINK( alias, cmd->first_alias, cmd->last_alias, next, prev );
               fMatch = TRUE;
               break;
            }
            break;

         case 'C':
            I3KEY( "Connected", cmd->connected, i3_fread_number( fp ) );
            if( !strcasecmp( word, "Code" ) )
            {
               word = i3_fread_word( fp );
               cmd->function = i3_function( word );
               if( cmd->function == NULL )
                  i3_bug( "i3_readcommand: Command %s loaded with invalid function. Set to NULL.", cmd->name );
               fMatch = TRUE;
               break;
            }
            break;

         case 'N':
            I3KEY( "Name", cmd->name, i3_fread_line( fp ) );
            break;

         case 'P':
            if( !strcasecmp( word, "Perm" ) )
            {
               word = i3_fread_word( fp );
               permvalue = get_permvalue( word );

               if( permvalue < 0 || permvalue > I3PERM_IMP )
               {
                  i3_bug( "i3_readcommand: Command %s loaded with invalid permission. Set to Imp.", cmd->name );
                  cmd->level = I3PERM_IMP;
               }
               else
                  cmd->level = permvalue;
               fMatch = TRUE;
               break;
            }
            break;
      }
      if( !fMatch )
         i3_bug( "i3_readcommand: no match: %s", word );
   }
}

bool I3_load_commands( void )
{
   FILE *fp;
   I3_CMD_DATA *cmd;

   first_i3_command = last_i3_command = NULL;

   i3_log( "%s", "Loading I3 command table..." );

   if( !( fp = fopen( I3_CMD_FILE, "r" ) ) )
   {
      i3_log( "%s", "No command table found." );
      return FALSE;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fp );
      if( letter == '*' )
      {
         i3fread_to_eol( fp );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_commands: # not found." );
         break;
      }

      word = i3_fread_word( fp );
      if( !strcasecmp( word, "COMMAND" ) )
      {
         I3CREATE( cmd, I3_CMD_DATA, 1 );
         I3_readcommand( cmd, fp );
         I3LINK( cmd, first_i3_command, last_i3_command, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_commands: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fp );
   return TRUE;
}

void I3_saverouters( void )
{
   FILE *fp;
   ROUTER_DATA *router;

   if( !( fp = fopen( I3_ROUTER_FILE, "w" ) ) )
   {
      i3_log( "%s", "Couldn't write to I3 router file." );
      return;
   }

   for( router = first_router; router; router = router->next )
   {
      fprintf( fp, "%s", "#ROUTER\n" );
      fprintf( fp, "Name %s\n", router->name );
      fprintf( fp, "IP   %s\n", router->ip );
      fprintf( fp, "Port %d\n", router->port );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

void I3_readrouter( ROUTER_DATA * router, FILE * fp )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : i3_fread_word( fp );
      fMatch = FALSE;

      switch ( word[0] )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fp );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;

         case 'I':
            I3KEY( "IP", router->ip, i3_fread_line( fp ) );
            break;

         case 'N':
            I3KEY( "Name", router->name, i3_fread_line( fp ) );
            break;

         case 'P':
            I3KEY( "Port", router->port, i3_fread_number( fp ) );
            break;
      }
      if( !fMatch )
         i3_bug( "i3_readrouter: no match: %s", word );
   }
}

bool I3_load_routers( void )
{
   FILE *fp;
   ROUTER_DATA *router;

   first_router = last_router = NULL;

   i3_log( "%s", "Loading I3 router data..." );

   if( !( fp = fopen( I3_ROUTER_FILE, "r" ) ) )
   {
      i3_log( "%s", "No router data found." );
      return FALSE;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fp );
      if( letter == '*' )
      {
         i3fread_to_eol( fp );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_routers: # not found." );
         break;
      }

      word = i3_fread_word( fp );
      if( !strcasecmp( word, "ROUTER" ) )
      {
         I3CREATE( router, ROUTER_DATA, 1 );
         I3_readrouter( router, fp );
         if( !router->name || router->name[0] == '\0' || !router->ip || router->ip[0] == '\0' || router->port <= 0 )
         {
            I3STRFREE( router->name );
            I3STRFREE( router->ip );
            I3DISPOSE( router );
         }
         else
            I3LINK( router, first_router, last_router, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_routers: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fp );

   if( !first_router || !last_router )
      return FALSE;

   return TRUE;
}

ROUTER_DATA *i3_find_router( const char *name )
{
   ROUTER_DATA *router;

   for( router = first_router; router; router = router->next )
   {
      if( !strcasecmp( router->name, name ) )
         return router;
   }
   return NULL;
}

void I3_readucache( UCACHE_DATA * user, FILE * fp )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : i3_fread_word( fp );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fp );
            break;

         case 'N':
            I3KEY( "Name", user->name, i3_fread_line( fp ) );
            break;

         case 'S':
            I3KEY( "Sex", user->gender, i3_fread_number( fp ) );
            break;

         case 'T':
            I3KEY( "Time", user->time, i3_fread_number( fp ) );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;
      }
      if( !fMatch )
         i3_bug( "I3_readucache: no match: %s", word );
   }
}

void i3_load_ucache( void )
{
   FILE *fp;
   UCACHE_DATA *user;

   first_ucache = last_ucache = NULL;

   i3_log( "%s", "Loading ucache data..." );

   if( !( fp = fopen( I3_UCACHE_FILE, "r" ) ) )
   {
      i3_log( "%s", "No ucache data found." );
      return;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fp );
      if( letter == '*' )
      {
         i3fread_to_eol( fp );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "I3_load_ucahe: # not found." );
         break;
      }

      word = i3_fread_word( fp );
      if( !strcasecmp( word, "UCACHE" ) )
      {
         I3CREATE( user, UCACHE_DATA, 1 );
         I3_readucache( user, fp );
         I3LINK( user, first_ucache, last_ucache, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_ucache: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fp );
   return;
}

void i3_save_config( void )
{
   FILE *fp;

   if( !( fp = fopen( I3_CONFIG_FILE, "w" ) ) )
   {
      i3_log( "%s", "Couldn't write to i3.config file." );
      return;
   }

   fprintf( fp, "%s", "$I3CONFIG\n\n" );
   fprintf( fp, "%s", "# This file will now allow you to use tildes.\n" );
   fprintf( fp, "%s", "# Set autoconnect to 1 to automatically connect at bootup.\n" );
   fprintf( fp, "%s", "# This information can be edited online using 'i3config'\n" );
   fprintf( fp, "thismud      %s\n", this_i3mud->name );
   fprintf( fp, "autoconnect  %d\n", this_i3mud->autoconnect );
   fprintf( fp, "telnet       %s\n", this_i3mud->telnet );
   fprintf( fp, "web          %s\n", this_i3mud->web );
   fprintf( fp, "adminemail   %s\n", this_i3mud->admin_email );
   fprintf( fp, "openstatus   %s\n", this_i3mud->open_status );
   if( this_i3mud->mud_type )
      fprintf( fp, "mudtype      %s\n", this_i3mud->mud_type );
   if( this_i3mud->base_mudlib )
      fprintf( fp, "baselib   %s\n", this_i3mud->base_mudlib );
   if( this_i3mud->mudlib && strcasecmp( this_i3mud->mudlib, this_i3mud->base_mudlib ) )
      fprintf( fp, "mudlib       %s\n", this_i3mud->mudlib );
   fprintf( fp, "minlevel     %d\n", this_i3mud->minlevel );
   fprintf( fp, "immlevel     %d\n", this_i3mud->immlevel );
   fprintf( fp, "adminlevel   %d\n", this_i3mud->adminlevel );
   fprintf( fp, "implevel     %d\n\n", this_i3mud->implevel );

   fprintf( fp, "%s", "\n# Information below this point cannot be edited online.\n" );
   fprintf( fp, "%s", "# The services provided by your mud.\n" );
   fprintf( fp, "%s", "# Do not turn things on unless you KNOW your mud properly supports them!\n" );
   fprintf( fp, "%s", "# Refer to http://cie.imaginary.com/protocols/intermud3.html for public packet specifications.\n" );
   fprintf( fp, "tell         %d\n", this_i3mud->tell );
   fprintf( fp, "beep         %d\n", this_i3mud->beep );
   fprintf( fp, "emoteto      %d\n", this_i3mud->emoteto );
   fprintf( fp, "who          %d\n", this_i3mud->who );
   fprintf( fp, "finger       %d\n", this_i3mud->finger );
   fprintf( fp, "locate       %d\n", this_i3mud->locate );
   fprintf( fp, "channel      %d\n", this_i3mud->channel );
   fprintf( fp, "news         %d\n", this_i3mud->news );
   fprintf( fp, "mail         %d\n", this_i3mud->mail );
   fprintf( fp, "file         %d\n", this_i3mud->file );
   fprintf( fp, "auth         %d\n", this_i3mud->auth );
   fprintf( fp, "ucache       %d\n\n", this_i3mud->ucache );
   fprintf( fp, "%s", "# Port numbers for OOB services. Leave as 0 if your mud does not support these.\n" );
   fprintf( fp, "smtp         %d\n", this_i3mud->smtp );
   fprintf( fp, "ftp          %d\n", this_i3mud->ftp );
   fprintf( fp, "nntp         %d\n", this_i3mud->nntp );
   fprintf( fp, "http         %d\n", this_i3mud->http );
   fprintf( fp, "pop3         %d\n", this_i3mud->pop3 );
   fprintf( fp, "rcp          %d\n", this_i3mud->rcp );
   fprintf( fp, "amrcp        %d\n", this_i3mud->amrcp );
   fprintf( fp, "%s", "end\n" );
   fprintf( fp, "%s", "$END\n" );
   I3FCLOSE( fp );
   return;
}

void I3_fread_config_file( FILE * fin )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fin ) ? "end" : i3_fread_word( fin );
      fMatch = FALSE;

      switch ( word[0] )
      {
         case '#':
            fMatch = TRUE;
            i3fread_to_eol( fin );
            break;
         case 'a':
            I3KEY( "adminemail", this_i3mud->admin_email, i3_fread_line( fin ) );
            I3KEY( "adminlevel", this_i3mud->adminlevel, i3_fread_number( fin ) );
            I3KEY( "amrcp", this_i3mud->amrcp, i3_fread_number( fin ) );
            I3KEY( "auth", this_i3mud->auth, i3_fread_number( fin ) );
            I3KEY( "autoconnect", this_i3mud->autoconnect, i3_fread_number( fin ) );
            break;

         case 'b':
            I3KEY( "baselib", this_i3mud->base_mudlib, i3_fread_line( fin ) );
            I3KEY( "beep", this_i3mud->beep, i3_fread_number( fin ) );
            break;

         case 'c':
            I3KEY( "channel", this_i3mud->channel, i3_fread_number( fin ) );
            break;

         case 'e':
            I3KEY( "emoteto", this_i3mud->emoteto, i3_fread_number( fin ) );
            if( !strcasecmp( word, "end" ) )
            {
#if defined(I3CHRONICLES)
               char lib_buf[LGST];
               /*
                * Adjust mudlib information based on already supplied info (mud.h). -Orion
                */
               snprintf( lib_buf, LGST, "%s %s.%s", CODEBASE_VERSION_TITLE, CODEBASE_VERSION_MAJOR, CODEBASE_VERSION_MINOR );
               I3STRFREE( this_i3mud->mudlib );
               this_i3mud->mudlib = STRALLOC( lib_buf );

               /*
                * Adjust base_mudlib information based on already supplied info (mud.h). -Orion
                */
               I3STRFREE( this_i3mud->base_mudlib );
               this_i3mud->base_mudlib = STRALLOC( CODEBASE_VERSION_TITLE );
#endif
               I3STRFREE( this_i3mud->driver );
               this_i3mud->driver = I3STRALLOC( I3DRIVER );

               /*
                * Convert to new router file 
                */
               if( first_router )
               {
                  I3_saverouters(  );
                  i3_save_config(  );
               }
               return;
            }
            break;

         case 'f':
            I3KEY( "file", this_i3mud->file, i3_fread_number( fin ) );
            I3KEY( "finger", this_i3mud->finger, i3_fread_number( fin ) );
            I3KEY( "ftp", this_i3mud->ftp, i3_fread_number( fin ) );
            break;

         case 'h':
            I3KEY( "http", this_i3mud->http, i3_fread_number( fin ) );
            break;

         case 'i':
            I3KEY( "immlevel", this_i3mud->immlevel, i3_fread_number( fin ) );
            I3KEY( "implevel", this_i3mud->implevel, i3_fread_number( fin ) );
            break;

         case 'l':
            I3KEY( "locate", this_i3mud->locate, i3_fread_number( fin ) );
            break;

         case 'm':
            I3KEY( "mail", this_i3mud->mail, i3_fread_number( fin ) );
            I3KEY( "minlevel", this_i3mud->minlevel, i3_fread_number( fin ) );
            I3KEY( "mudlib", this_i3mud->mudlib, i3_fread_line( fin ) );
            I3KEY( "mudtype", this_i3mud->mud_type, i3_fread_line( fin ) );
            break;

         case 'n':
            I3KEY( "news", this_i3mud->news, i3_fread_number( fin ) );
            I3KEY( "nntp", this_i3mud->nntp, i3_fread_number( fin ) );
            break;

         case 'o':
            I3KEY( "openstatus", this_i3mud->open_status, i3_fread_line( fin ) );
            break;

         case 'p':
            I3KEY( "pop3", this_i3mud->pop3, i3_fread_number( fin ) );
            break;

         case 'r':
            I3KEY( "rcp", this_i3mud->rcp, i3_fread_number( fin ) );
            /*
             * Router config loading is legacy support here - routers are configured in their own file now. 
             */
            if( !strcasecmp( word, "router" ) )
            {
               ROUTER_DATA *router;
               char rname[SMST], rip[SMST], *ln;
               int rport;

               ln = i3_fread_line( fin );
               sscanf( ln, "%s %s %d", rname, rip, &rport );

               I3CREATE( router, ROUTER_DATA, 1 );
               router->name = I3STRALLOC( rname );
               router->ip = I3STRALLOC( rip );
               router->port = rport;
               router->reconattempts = 0;
               I3LINK( router, first_router, last_router, next, prev );
               fMatch = TRUE;
               I3DISPOSE( ln );
               break;
            }
            break;

         case 's':
            I3KEY( "smtp", this_i3mud->smtp, i3_fread_number( fin ) );
            break;

         case 't':
            I3KEY( "tell", this_i3mud->tell, i3_fread_number( fin ) );
            I3KEY( "telnet", this_i3mud->telnet, i3_fread_line( fin ) );
            I3KEY( "thismud", this_i3mud->name, i3_fread_line( fin ) );
            break;

         case 'u':
            I3KEY( "ucache", this_i3mud->ucache, i3_fread_number( fin ) );
            break;

         case 'w':
            I3KEY( "web", this_i3mud->web, i3_fread_line( fin ) );
            I3KEY( "who", this_i3mud->who, i3_fread_number( fin ) );
            break;
      }
      if( !fMatch )
         i3_bug( "I3_fread_config_file: Bad keyword: %s\r\n", word );
   }
}

bool i3_read_config( int mudport )
{
   FILE *fin, *fp;

   if( this_i3mud != NULL )
      destroy_I3_mud( this_i3mud );
   this_i3mud = NULL;

   i3_log( "%s", "Loading Intermud-3 network data..." );

   if( !( fin = fopen( I3_CONFIG_FILE, "r" ) ) )
   {
      i3_log( "%s", "Can't open configuration file: i3.config" );
      i3_log( "%s", "Network configuration aborted." );
      return FALSE;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fin );

      if( letter == '#' )
      {
         i3fread_to_eol( fin );
         continue;
      }

      if( letter != '$' )
      {
         i3_bug( "%s", "i3_read_config: $ not found" );
         break;
      }

      word = i3_fread_word( fin );
      if( !strcasecmp( word, "I3CONFIG" ) && this_i3mud == NULL )
      {
         I3CREATE( this_i3mud, I3_MUD, 1 );

         this_i3mud->status = -1;
         this_i3mud->autoconnect = 0;
         this_i3mud->player_port = mudport;  /* Passed in from the mud's startup script */
         this_i3mud->password = 0;
         this_i3mud->mudlist_id = 0;
         this_i3mud->chanlist_id = 0;
         this_i3mud->minlevel = 10; /* Minimum default level before I3 will acknowledge you exist */
         this_i3mud->immlevel = 103;   /* Default immortal level */
         this_i3mud->adminlevel = 113; /* Default administration level */
         this_i3mud->implevel = 115;   /* Default implementor level */

         I3_fread_config_file( fin );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_read_config: Bad section in config file: %s", word );
         continue;
      }
   }
   I3FCLOSE( fin );

   if( !this_i3mud )
   {
      i3_log( "%s", "Error during configuration load." );
      i3_log( "%s", "Network configuration aborted." );
      return FALSE;
   }

   if( ( fp = fopen( I3_PASSWORD_FILE, "r" ) ) != NULL && this_i3mud != NULL )
   {
      char *word;

      word = i3_fread_word( fp );

      if( !strcasecmp( word, "#PASSWORD" ) )
      {
         char *ln = i3_fread_line( fp );
         int pass, mud, chan;

         pass = mud = chan = 0;
         sscanf( ln, "%d %d %d", &pass, &mud, &chan );
         this_i3mud->password = pass;
         this_i3mud->mudlist_id = mud;
         this_i3mud->chanlist_id = chan;
         I3DISPOSE( ln );
      }
      I3FCLOSE( fp );
   }

   if( !this_i3mud->name || this_i3mud->name[0] == '\0' )
   {
      i3_log( "%s", "Mud name not loaded in configuration file." );
      i3_log( "%s", "Network configuration aborted." );
      destroy_I3_mud( this_i3mud );
      return FALSE;
   }

   if( !this_i3mud->telnet || this_i3mud->telnet[0] == '\0' )
      this_i3mud->telnet = I3STRALLOC( "Address not configured" );

   if( !this_i3mud->web || this_i3mud->web[0] == '\0' )
      this_i3mud->web = I3STRALLOC( "Address not configured" );

   I3_THISMUD = this_i3mud->name;
   return TRUE;
}

void I3_readban( I3_BAN * ban, FILE * fin )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fin ) ? "End" : i3_fread_word( fin );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fin );
            break;

         case 'N':
            I3KEY( "Name", ban->name, i3_fread_line( fin ) );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
               return;
            break;
      }
      if( !fMatch )
         i3_bug( "I3_readban: no match: %s", word );
   }
}

void i3_load_bans( void )
{
   FILE *fin;
   I3_BAN *ban;

   first_i3ban = NULL;
   last_i3ban = NULL;

   i3_log( "%s", "Loading ban list..." );

   if( ( fin = fopen( I3_BAN_FILE, "r" ) ) == NULL )
   {
      i3_log( "%s", "No ban list defined." );
      return;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fin );
      if( letter == '*' )
      {
         i3fread_to_eol( fin );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_bans: # not found." );
         break;
      }

      word = i3_fread_word( fin );
      if( !strcasecmp( word, "I3BAN" ) )
      {
         I3CREATE( ban, I3_BAN, 1 );
         I3_readban( ban, fin );
         if( !ban->name )
            I3DISPOSE( ban );
         else
            I3LINK( ban, first_i3ban, last_i3ban, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_bans: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fin );
   return;
}

void I3_write_bans( void )
{
   FILE *fout;
   I3_BAN *ban;

   if( ( fout = fopen( I3_BAN_FILE, "w" ) ) == NULL )
   {
      i3_log( "%s", "Couldn't write to ban list file." );
      return;
   }

   for( ban = first_i3ban; ban; ban = ban->next )
   {
      fprintf( fout, "%s", "#I3BAN\n" );
      fprintf( fout, "Name   %s\n", ban->name );
      fprintf( fout, "%s", "End\n\n" );
   }
   fprintf( fout, "%s", "#END\n" );
   I3FCLOSE( fout );
}

void i3_read_channel( I3_CHANNEL * channel, FILE * fin )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fin ) ? "End" : i3_fread_word( fin );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fin );
            break;

         case 'C':
            I3KEY( "ChanName", channel->I3_name, i3_fread_line( fin ) );
            I3KEY( "ChanMud", channel->host_mud, i3_fread_line( fin ) );
            I3KEY( "ChanLocal", channel->local_name, i3_fread_line( fin ) );
            I3KEY( "ChanLayM", channel->layout_m, i3_fread_line( fin ) );
            I3KEY( "ChanLayE", channel->layout_e, i3_fread_line( fin ) );
            I3KEY( "ChanLevel", channel->i3perm, i3_fread_number( fin ) );
            I3KEY( "ChanStatus", channel->status, i3_fread_number( fin ) );
            I3KEY( "ChanFlags", channel->flags, i3_fread_number( fin ) );
            break;

         case 'E':
            if( !strcasecmp( word, "End" ) )
            {
               /*
                * Legacy support to convert channel permissions 
                */
               if( channel->i3perm > I3PERM_IMP )
               {
                  /*
                   * The I3PERM_NONE condition should realistically never happen.... 
                   */
                  if( channel->i3perm < this_i3mud->minlevel )
                     channel->i3perm = I3PERM_NONE;
                  else if( channel->i3perm >= this_i3mud->minlevel && channel->i3perm < this_i3mud->immlevel )
                     channel->i3perm = I3PERM_MORT;
                  else if( channel->i3perm >= this_i3mud->immlevel && channel->i3perm < this_i3mud->adminlevel )
                     channel->i3perm = I3PERM_IMM;
                  else if( channel->i3perm >= this_i3mud->adminlevel && channel->i3perm < this_i3mud->implevel )
                     channel->i3perm = I3PERM_ADMIN;
                  else if( channel->i3perm >= this_i3mud->implevel )
                     channel->i3perm = I3PERM_IMP;
               }
               /* Also, add a few other sanity checks here */

               return;
            }
            break;
      }
      if( !fMatch )
         i3_bug( "i3_read_channel: no match: %s", word );
   }
}

void i3_load_channels( void )
{
   FILE *fin;
   I3_CHANNEL *channel;

   first_I3chan = last_I3chan = NULL;

   i3_log( "%s", "Loading channels..." );

   if( !( fin = fopen( I3_CHANNEL_FILE, "r" ) ) )
   {
      i3_log( "%s", "No channel config file found." );
      return;
   }

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fin );
      if( letter == '*' )
      {
         i3fread_to_eol( fin );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_channels: # not found." );
         break;
      }

      word = i3_fread_word( fin );
      if( !strcasecmp( word, "I3CHAN" ) )
      {
         int x;

         I3CREATE( channel, I3_CHANNEL, 1 );
         i3_read_channel( channel, fin );

         if(channel->local_name && !strcmp(channel->local_name, "(null)")) {
            I3STRFREE( channel->local_name );
            channel->local_name = NULL;
         }
         if(channel->layout_m && !strcmp(channel->layout_m, "(null)")) {
            I3STRFREE( channel->layout_m );
            channel->layout_m = I3STRALLOC( "&R[&W%s&R] &C%s@%s: &c%s" );
         }
         if(channel->layout_e && !strcmp(channel->layout_e, "(null)")) {
            I3STRFREE( channel->layout_e );
            channel->layout_e = I3STRALLOC( "&R[&W%s&R] &c%s" );
         }

         for( x = 0; x < MAX_I3HISTORY; x++ )
            channel->history[x] = NULL;
         I3LINK( channel, first_I3chan, last_I3chan, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_channels: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fin );
   return;
}

void i3_write_channel_config( void )
{
   FILE *fout;
   I3_CHANNEL *channel;

   if( ( fout = fopen( I3_CHANNEL_FILE, "w" ) ) == NULL )
   {
      i3_log( "%s", "Couldn't write to channel config file." );
      return;
   }

   for( channel = first_I3chan; channel; channel = channel->next )
   {
       /* Save ALL channels... we don't want to have to wait
        * to rebuild the list from I3 every frigging reboot
        */
      //if( channel->local_name )
      //{
         fprintf( fout, "%s", "#I3CHAN\n" );
         fprintf( fout, "ChanName   %s\n", channel->I3_name );
         fprintf( fout, "ChanMud    %s\n", channel->host_mud );
         fprintf( fout, "ChanLocal  %s\n", channel->local_name ? channel->local_name : "(null)" );
         fprintf( fout, "ChanLayM   %s\n", channel->layout_m ? channel->layout_m : "(null)" );
         fprintf( fout, "ChanLayE   %s\n", channel->layout_e ? channel->layout_e : "(null)" );
         fprintf( fout, "ChanLevel  %d\n", channel->i3perm );
         fprintf( fout, "ChanStatus %d\n", channel->status );
         fprintf( fout, "ChanFlags  %ld\n", ( long int )channel->flags );
         fprintf( fout, "%s", "End\n\n" );
      //}
   }
   fprintf( fout, "%s", "#END\n" );
   I3FCLOSE( fout );
}

/* Used only during copyovers */
void fread_mudlist( FILE * fin, I3_MUD * mud )
{
   const char *word;
   char *ln;
   bool fMatch;
   int x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12;

   for( ;; )
   {
      word = feof( fin ) ? "End" : i3_fread_word( fin );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            i3fread_to_eol( fin );
            break;

         case 'B':
            I3KEY( "Banner", mud->banner, i3_fread_line( fin ) );
            I3KEY( "Baselib", mud->base_mudlib, i3_fread_line( fin ) );
            break;

         case 'D':
            I3KEY( "Daemon", mud->daemon, i3_fread_line( fin ) );
            I3KEY( "Driver", mud->driver, i3_fread_line( fin ) );
            break;

         case 'E':
            I3KEY( "Email", mud->admin_email, i3_fread_line( fin ) );
            if( !strcasecmp( word, "End" ) )
            {
               return;
            }

         case 'I':
            I3KEY( "IP", mud->ipaddress, i3_fread_line( fin ) );
            break;

         case 'M':
            I3KEY( "Mudlib", mud->mudlib, i3_fread_line( fin ) );
            break;

         case 'O':
            I3KEY( "Openstatus", mud->open_status, i3_fread_line( fin ) );
            if( !strcasecmp( word, "OOBPorts" ) )
            {
               ln = i3_fread_line( fin );
               x1 = x2 = x3 = x4 = x5 = x6 = x7 = 0;

               sscanf( ln, "%d %d %d %d %d %d %d", &x1, &x2, &x3, &x4, &x5, &x6, &x7 );
               mud->smtp = x1;
               mud->ftp = x2;
               mud->nntp = x3;
               mud->http = x4;
               mud->pop3 = x5;
               mud->rcp = x6;
               mud->amrcp = x7;
               fMatch = TRUE;
               I3DISPOSE( ln );
               break;
            }
            break;

         case 'P':
            if( !strcasecmp( word, "Ports" ) )
            {
               ln = i3_fread_line( fin );
               x1 = x2 = x3 = 0;

               sscanf( ln, "%d %d %d ", &x1, &x2, &x3 );
               mud->player_port = x1;
               mud->imud_tcp_port = x2;
               mud->imud_udp_port = x3;
               fMatch = TRUE;
               I3DISPOSE( ln );
               break;
            }
            break;

         case 'S':
            I3KEY( "Status", mud->status, i3_fread_number( fin ) );
            if( !strcasecmp( word, "Services" ) )
            {
               ln = i3_fread_line( fin );
               x1 = x2 = x3 = x4 = x5 = x6 = x7 = x8 = x9 = x10 = x11 = x12 = 0;

               sscanf( ln, "%d %d %d %d %d %d %d %d %d %d %d %d",
                       &x1, &x2, &x3, &x4, &x5, &x6, &x7, &x8, &x9, &x10, &x11, &x12 );
               mud->tell = x1;
               mud->beep = x2;
               mud->emoteto = x3;
               mud->who = x4;
               mud->finger = x5;
               mud->locate = x6;
               mud->channel = x7;
               mud->news = x8;
               mud->mail = x9;
               mud->file = x10;
               mud->auth = x11;
               mud->ucache = x12;
               fMatch = TRUE;
               I3DISPOSE( ln );
               break;
            }
            break;

         case 'T':
            I3KEY( "Telnet", mud->telnet, i3_fread_line( fin ) );
            I3KEY( "Time", mud->time, i3_fread_line( fin ) );
            I3KEY( "Type", mud->mud_type, i3_fread_line( fin ) );
            break;

         case 'W':
            I3KEY( "Web", mud->web, i3_fread_line( fin ) );
            break;
      }

      if( !fMatch )
         i3_bug( "I3_readmudlist: no match: %s", word );
   }
}

/* Called only during copyovers */
void i3_load_mudlist( void )
{
   FILE *fin;
   I3_MUD *mud;

   if( !( fin = fopen( I3_MUDLIST_FILE, "r" ) ) )
      return;

   for( ;; )
   {
      char letter;
      const char *word;

      letter = i3fread_letter( fin );
      if( letter == '*' )
      {
         i3fread_to_eol( fin );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_mudlist: # not found." );
         break;
      }

      word = i3_fread_word( fin );
      if( !strcasecmp( word, "ROUTER" ) )
      {
         I3STRFREE( this_i3mud->routerName );
         this_i3mud->routerName = i3_fread_line( fin );
         I3_ROUTER_NAME = this_i3mud->routerName;
         continue;
      }
      if( !strcasecmp( word, "MUDLIST" ) )
      {
         word = i3_fread_word( fin );
         if( !strcasecmp( word, "Name" ) )
         {
            char *tmpname;

            tmpname = i3_fread_line( fin );
            mud = new_I3_mud( tmpname );
            fread_mudlist( fin, mud );
            I3STRFREE( tmpname );
         }
         else
         {
            i3_bug( "%s", "fread_mudlist: No mudname saved, skipping entry." );
            i3fread_to_eol( fin );
            for( ;; )
            {
               word = feof( fin ) ? "End" : i3_fread_word( fin );
               if( strcasecmp( word, "End" ) )
                  i3fread_to_eol( fin );
               else
                  break;
            }
         }
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_mudlist: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fin );
   unlink( I3_MUDLIST_FILE );
   return;
}

/* Called only during copyovers */
void i3_load_chanlist( void )
{
   FILE *fin;
   I3_CHANNEL *channel;

   if( !( fin = fopen( I3_CHANLIST_FILE, "r" ) ) )
      return;

   for( ;; )
   {
      char letter;
      char *word;

      letter = i3fread_letter( fin );
      if( letter == '*' )
      {
         i3fread_to_eol( fin );
         continue;
      }

      if( letter != '#' )
      {
         i3_bug( "%s", "i3_load_chanlist: # not found." );
         break;
      }

      word = i3_fread_word( fin );
      if( !strcasecmp( word, "I3CHAN" ) )
      {
         int x;
         I3CREATE( channel, I3_CHANNEL, 1 );
         i3_read_channel( channel, fin );

         for( x = 0; x < MAX_I3HISTORY; x++ )
            channel->history[x] = NULL;
         I3LINK( channel, first_I3chan, last_I3chan, next, prev );
         continue;
      }
      else if( !strcasecmp( word, "END" ) )
         break;
      else
      {
         i3_bug( "i3_load_chanlist: bad section: %s.", word );
         continue;
      }
   }
   I3FCLOSE( fin );
   unlink( I3_CHANLIST_FILE );
   return;
}

/* Called only during copyovers */
void i3_save_mudlist( void )
{
   FILE *fp;
   I3_MUD *mud;

   if( !( fp = fopen( I3_MUDLIST_FILE, "w" ) ) )
   {
      i3_bug( "%s", "i3_save_mudlist: Unable to write to mudlist file." );
      return;
   }

   fprintf( fp, "#ROUTER %s\n", I3_ROUTER_NAME );
   for( mud = first_mud; mud; mud = mud->next )
   {
      /*
       * Don't store muds that are down, who cares? They'll update themselves anyway 
       */
      if( mud->status == 0 )
         continue;

      fprintf( fp, "%s", "#MUDLIST\n" );
      fprintf( fp, "Name		%s\n", mud->name );
      fprintf( fp, "Status		%d\n", mud->status );
      fprintf( fp, "IP			%s\n", mud->ipaddress );
      fprintf( fp, "Mudlib		%s\n", mud->mudlib );
      fprintf( fp, "Baselib		%s\n", mud->base_mudlib );
      fprintf( fp, "Driver		%s\n", mud->driver );
      fprintf( fp, "Type		%s\n", mud->mud_type );
      fprintf( fp, "Openstatus	%s\n", mud->open_status );
      fprintf( fp, "Email		%s\n", mud->admin_email );
      if( mud->telnet )
         fprintf( fp, "Telnet		%s\n", mud->telnet );
      if( mud->web )
         fprintf( fp, "Web		%s\n", mud->web );
      if( mud->banner )
         fprintf( fp, "Banner		%s\n", mud->banner );
      if( mud->daemon )
         fprintf( fp, "Dameon		%s\n", mud->daemon );
      if( mud->time )
         fprintf( fp, "Time		%s\n", mud->time );
      fprintf( fp, "Ports %d %d %d\n", mud->player_port, mud->imud_tcp_port, mud->imud_udp_port );
      fprintf( fp, "Services %d %d %d %d %d %d %d %d %d %d %d %d\n",
               mud->tell, mud->beep, mud->emoteto, mud->who, mud->finger, mud->locate, mud->channel, mud->news, mud->mail,
               mud->file, mud->auth, mud->ucache );
      fprintf( fp, "OOBports %d %d %d %d %d %d %d\n", mud->smtp, mud->ftp, mud->nntp, mud->http, mud->pop3, mud->rcp,
               mud->amrcp );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

/* File:
 * #I3CHAN
 * ChanName   dchat
 * ChanMud    Frontiers
 * ChanLocal  dchat
 * ChanLayM   &R[&W%s&R] &C%s@%s: &c%s
 * ChanLayE   &R[&W%s&R] &c%s
 * ChanLevel  2
 * ChanStatus 0
 * ChanFlags  0
 * End
 */
/* Called only during copyovers */
void i3_save_chanlist( void )
{
   FILE *fp;
   I3_CHANNEL *channel;

   if( !( fp = fopen( I3_CHANLIST_FILE, "w" ) ) )
   {
      i3_bug( "%s", "i3_save_chanlist: Unable to write to chanlist file." );
      return;
   }

   for( channel = first_I3chan; channel; channel = channel->next )
   {
      /*
       * Don't save local channels, they are stored elsewhere 
       */
      if( channel->local_name )
         continue;

      fprintf( fp, "%s", "#I3CHAN\n" );
      fprintf( fp, "ChanMud		%s\n", channel->host_mud );
      fprintf( fp, "ChanName		%s\n", channel->I3_name );
      fprintf( fp, "ChanStatus	%d\n", channel->status );
      fprintf( fp, "%s", "End\n\n" );
   }
   fprintf( fp, "%s", "#END\n" );
   I3FCLOSE( fp );
   return;
}

/* Used during copyovers */
void I3_loadhistory( void )
{
   char filename[SMST];
   FILE *tempfile;
   I3_CHANNEL *tempchan = NULL;
   int x;

   for( tempchan = first_I3chan; tempchan; tempchan = tempchan->next )
   {
      if( !tempchan->local_name )
         continue;

      if( !strcasecmp(tempchan->local_name, "(null)") )
         continue;

      snprintf( filename, SMST, "%s%s.hist", I3_DIR, tempchan->local_name );

      if( !( tempfile = fopen( filename, "r" ) ) )
         continue;

      for( x = 0; x < MAX_I3HISTORY; x++ )
      {
         if( feof( tempfile ) )
            tempchan->history[x] = NULL;
         else
            tempchan->history[x] = i3_fread_line( tempfile );
      }
      I3FCLOSE( tempfile );
      unlink( filename );
   }
}

/* Used during copyovers */
void i3_save_history( void )
{
   char filename[SMST];
   FILE *tempfile;
   I3_CHANNEL *tempchan = NULL;
   int x;

   for( tempchan = first_I3chan; tempchan; tempchan = tempchan->next )
   {
      if( !tempchan->local_name )
         continue;

      if( !strcasecmp(tempchan->local_name, "(null)") )
         continue;

      if( !tempchan->history[0] )
         continue;

      snprintf( filename, SMST, "%s%s.hist", I3_DIR, tempchan->local_name );

      if( !( tempfile = fopen( filename, "w" ) ) )
         continue;

      for( x = 0; x < MAX_I3HISTORY; x++ )
      {
         if( tempchan->history[x] != NULL )
            fprintf( tempfile, "%s\n", tempchan->history[x] );
      }
      I3FCLOSE( tempfile );
   }
}

/*
 * Setup a TCP session to the router. Returns socket or <0 if failed.
 *
 */
int i3_connection_open( ROUTER_DATA * router )
{
   struct sockaddr_in sa;
   struct hostent *hostp;
   int x = 1;

   i3_log( "Attempting connect to %s on port %d", router->ip, router->port );

   I3_socket = socket( AF_INET, SOCK_STREAM, 0 );
   if( I3_socket < 0 )
   {
      i3_log( "%s", "Cannot create socket!" );
      i3_close_connection( TRUE );
      return -1;
   }

   if( ( x = fcntl( I3_socket, F_GETFL, 0 ) ) < 0 )
   {
      i3_log( "%s", "i3_connection_open: fcntl(F_GETFL)" );
      i3_close_connection( TRUE );
      return -1;
   }

   if( fcntl( I3_socket, F_SETFL, x | O_NONBLOCK ) < 0 )
   {
      i3_log( "%s", "i3_connection_open: fcntl(F_SETFL)" );
      i3_close_connection( TRUE );
      return -1;
   }

   memset( &sa, 0, sizeof( sa ) );
   sa.sin_family = AF_INET;

   if( !inet_aton( router->ip, &sa.sin_addr ) )
   {
      hostp = gethostbyname( router->ip );
      if( !hostp )
      {
         i3_log( "%s", "i3_connection_open: Cannot resolve router hostname." );
         i3_close_connection( TRUE );
         return -1;
      }
      memcpy( &sa.sin_addr, hostp->h_addr, hostp->h_length );
   }

   sa.sin_port = htons( router->port );

   if( connect( I3_socket, ( struct sockaddr * )&sa, sizeof( sa ) ) < 0 )
   {
      if( errno != EINPROGRESS )
      {
         i3_log( "i3_connection_open: Unable to connect to router %s", router->name );
         i3_close_connection( TRUE );
         return -1;
      }
   }
   I3_ROUTER_NAME = router->name;
   i3_log( "Connected to Intermud-3 router %s", router->name );
   return I3_socket;
}

/*
 * Close the socket to the router.
 */
void i3_close_connection( bool reconnect )
{
   ROUTER_DATA *router = NULL;
   bool rfound = FALSE;

   for( router = first_router; router; router = router->next )
      if( !strcasecmp( router->name, I3_ROUTER_NAME ) )
      {
         rfound = TRUE;
         break;
      }

   if( !rfound )
   {
      i3_log( "%s", "i3_close_connection: Disconnecting from router." );
      if( I3_socket > 0 )
      {
         close( I3_socket );
         I3_socket = -1;
      }
      return;
   }

   i3_log( "Closing connection to Intermud-3 router %s", router->name );
   if( I3_socket > 0 )
   {
      close( I3_socket );
      I3_socket = -1;
   }
   if( reconnect )
   {
      if( router->reconattempts <= 3 )
      {
         i3wait = 100;  /* Wait for 100 game loops */
         i3_log( "%s", "Will attempt to reconnect in approximately 15 seconds." );
      }
      else if( router->next != NULL )
      {
         i3_log( "Unable to reach %s. Abandoning connection.", router->name );
         i3_log( "Bytes sent: %ld. Bytes received: %ld.", bytes_sent, bytes_received );
         bytes_sent = 0;
         bytes_received = 0;
         i3wait = 100;
         i3_log( "Will attempt new connection to %s in approximately 15 seconds.", router->next->name );
      }
      else
      {
         bytes_sent = 0;
         bytes_received = 0;
         i3wait = -2;
         i3_log( "%s", "Unable to reconnect. No routers responding." );
         return;
      }
   }
   i3_log( "Bytes sent: %ld. Bytes received: %ld.", bytes_sent, bytes_received );
   bytes_sent = 0;
   bytes_received = 0;
   return;
}

/* Free up all the data lists once the connection is down. No sense in wasting memory on it. */
void i3_free_data( bool complete )
{
   I3_MUD *mud, *next_mud;
   I3_CHANNEL *channel, *next_chan;
   I3_BAN *ban, *next_ban;
   UCACHE_DATA *ucache, *next_ucache;
   ROUTER_DATA *router, *router_next;
   I3_CMD_DATA *cmd, *cmd_next;
   I3_ALIAS *alias, *alias_next;
   I3_HELP_DATA *help, *help_next;
   I3_COLOR *color, *color_next;

   if( first_i3ban )
   {
      for( ban = first_i3ban; ban; ban = next_ban )
      {
         next_ban = ban->next;
         I3STRFREE( ban->name );
         I3UNLINK( ban, first_i3ban, last_i3ban, next, prev );
         I3DISPOSE( ban );
      }
   }

   if( first_I3chan )
   {
      for( channel = first_I3chan; channel; channel = next_chan )
      {
         next_chan = channel->next;
         destroy_I3_channel( channel );
      }
   }

   if( first_mud )
   {
      for( mud = first_mud; mud; mud = next_mud )
      {
         next_mud = mud->next;
         destroy_I3_mud( mud );
      }
   }

   if( first_ucache )
   {
      for( ucache = first_ucache; ucache; ucache = next_ucache )
      {
         next_ucache = ucache->next;
         I3STRFREE( ucache->name );
         I3UNLINK( ucache, first_ucache, last_ucache, next, prev );
         I3DISPOSE( ucache );
      }
   }

   if( complete == TRUE )
   {
      for( router = first_router; router; router = router_next )
      {
         router_next = router->next;
         I3STRFREE( router->name );
         I3STRFREE( router->ip );
         I3UNLINK( router, first_router, last_router, next, prev );
         I3DISPOSE( router );
      }

      for( cmd = first_i3_command; cmd; cmd = cmd_next )
      {
         cmd_next = cmd->next;

         for( alias = cmd->first_alias; alias; alias = alias_next )
         {
            alias_next = alias->next;

            I3STRFREE( alias->name );
            I3UNLINK( alias, cmd->first_alias, cmd->last_alias, next, prev );
            I3DISPOSE( alias );
         }
         I3STRFREE( cmd->name );
         I3UNLINK( cmd, first_i3_command, last_i3_command, next, prev );
         I3DISPOSE( cmd );
      }

      for( help = first_i3_help; help; help = help_next )
      {
         help_next = help->next;
         I3STRFREE( help->name );
         I3STRFREE( help->text );
         I3UNLINK( help, first_i3_help, last_i3_help, next, prev );
         I3DISPOSE( help );
      }

      for( color = first_i3_color; color; color = color_next )
      {
         color_next = color->next;
         I3STRFREE( color->name );
         I3STRFREE( color->mudtag );
         I3STRFREE( color->i3tag );
         I3STRFREE( color->i3fish );
         I3UNLINK( color, first_i3_color, last_i3_color, next, prev );
         I3DISPOSE( color );
      }
   }
   return;
}

/*
 * Shutdown the connection to the router.
 */
void i3_shutdown( int delay )
{
   if( I3_socket < 1 )
      return;

   i3_save_history(  );
   i3_free_data( FALSE );

   /*
    * Flush the outgoing buffer 
    */
   if( I3_output_pointer != 4 )
      i3_write_packet( I3_output_buffer );

   i3_send_shutdown( delay );
   i3_close_connection( FALSE );
   I3_input_pointer = 0;
   I3_output_pointer = 4;
   i3_save_id(  );
   sleep( 2 ); /* Short delay to allow the socket to close */
}

/*
 * Connect to the router and send the startup-packet.
 * Mud port is passed in from main() so that the information passed along to the I3
 * network regarding the mud's operational port is now determined by the mud's own
 * startup script instead of the I3 config file.
 */
void router_connect( const char *router_name, bool forced, int mudport, bool isconnected )
{
   ROUTER_DATA *router;
   bool rfound = FALSE;

   i3wait = 0;
   i3timeout = 0;
   bytes_sent = 0;
   bytes_received = 0;

   manual_router = router_name;

   /*
    * The Command table is required for operation. Like.... duh? 
    */
   if( first_i3_command == NULL )
   {
      if( !I3_load_commands(  ) )
      {
         i3_log( "%s", "router_connect: Unable to load command table!" );
         I3_socket = -1;
         return;
      }
   }

   if( !i3_read_config( mudport ) )
   {
      I3_socket = -1;
      return;
   }

   if( first_router == NULL )
   {
      if( !I3_load_routers(  ) )
      {
         i3_log( "%s", "router_connect: No router configurations were found!" );
         I3_socket = -1;
         return;
      }
      I3_ROUTER_NAME = first_router->name;
   }

   /*
    * Help information should persist even when the network is not connected... 
    */
   if( first_i3_help == NULL )
      I3_load_helps(  );

   /*
    * ... as should the color table. 
    */
   if( first_i3_color == NULL )
      i3_load_color_table(  );

   if( ( !this_i3mud->autoconnect && !forced && !isconnected ) || ( isconnected && I3_socket < 1 ) )
   {
      i3_log( "%s", "Intermud-3 network data loaded. Autoconnect not set. Will need to connect manually." );
      I3_socket = -1;
      return;
   }
   else
      i3_log( "%s", "Intermud-3 network data loaded. Initialiazing network connection..." );

   i3_load_channels(  );
   i3_load_bans(  );

   if( this_i3mud->ucache == TRUE )
   {
      i3_load_ucache(  );
      i3_prune_ucache(  );
      ucache_clock = i3_time + 86400;
   }

   if( I3_socket < 1 )
   {
      for( router = first_router; router; router = router->next )
      {
         if( router_name && strcasecmp( router_name, router->name ) )
            continue;

         if( router->reconattempts <= 3 )
         {
            rfound = TRUE;
            I3_socket = i3_connection_open( router );
            break;
         }
      }
   }

   if( !rfound && !isconnected )
   {
      i3_log( "%s", "Unable to connect. No available routers found." );
      I3_socket = -1;
      return;
   }

   if( I3_socket < 1 )
   {
      i3wait = 100;
      return;
   }

   sleep( 1 );

   i3_log( "%s", "Intermud-3 Network initialized." );

   if( !isconnected )
   {
      i3_startup_packet(  );
      i3timeout = 100;
   }
   else
   {
      i3_load_mudlist(  );
      i3_load_chanlist(  );
   }
   I3_loadhistory(  );
}

/* Wrapper for router_connect now - so we don't need to force older client installs to adjust. */
void i3_startup( bool forced, int mudport, bool isconnected )
{
   if(i3_read_config(mudport))
      router_connect( NULL, forced, mudport, isconnected );
   else
      i3_bug("i3_startup: %s", "Configuration failed!");
}

/*
 * Check for a packet and if one available read it and parse it.
 * Also checks to see if the mud should attempt to reconnect to the router.
 * This is an input only loop. Attempting to use it to send buffered output
 * just wasn't working out, so output has gone back to sending packets to the
 * router as soon as they're assembled.
 */
void i3_loop( void )
{
   ROUTER_DATA *router;
   int ret;
   long size;
   fd_set in_set, out_set, exc_set;
   static struct timeval null_time;
#ifdef I3_WILEY
   static struct last_time;
#endif
   bool rfound = FALSE;
   size_t space_left = 0;

#ifdef I3CIRCLE
   i3_time = time( NULL );
#endif
#ifdef I3_WILEY
   gettimeofday( &last_time, NULL );
   i3_time = ( time_t ) last_time.tv_sec;
#endif

   FD_ZERO( &in_set );
   FD_ZERO( &out_set );
   FD_ZERO( &exc_set );

   if( i3wait > 0 )
      i3wait--;

   if( i3timeout > 0 )
   {
      i3timeout--;
      if( i3timeout == 0 ) /* Time's up baby! */
      {
         i3_close_connection( TRUE );
         return;
      }
   }

   /*
    * This condition can only occur if you were previously connected and the socket was closed.
    * * Tries 3 times, then attempts connection to an alternate router, if it has one.
    */
   if( i3wait == 1 )
   {
      for( router = first_router; router; router = router->next )
      {
         if( manual_router && strcasecmp( router->name, manual_router ) )
            continue;

         if( router->reconattempts <= 3 )
         {
            rfound = TRUE;
            break;
         }
      }

      if( !rfound )
      {
         i3wait = -2;
         i3_log( "%s", "Unable to reconnect. No routers responding." );
         return;
      }
      I3_socket = i3_connection_open( router );
      if( I3_socket < 1 )
      {
         if( router->reconattempts <= 3 )
            i3wait = 100;
         return;
      }

      sleep( 1 );

      i3_log( "Connection to Intermud-3 router %s %s.",
             router->name, router->reconattempts > 0 ? "reestablished" : "established" );
      router->reconattempts++;
      i3_startup_packet(  );
      i3timeout = 100;
      return;
   }

   if( !i3_is_connected(  ) )
      return;

   /*
    * Will prune the cache once every 24hrs after bootup time 
    */
   if( ucache_clock <= i3_time )
   {
      ucache_clock = i3_time + 86400;
      i3_prune_ucache(  );
   }

   FD_SET( I3_socket, &in_set );
   FD_SET( I3_socket, &out_set );
   FD_SET( I3_socket, &exc_set );

   if( select( I3_socket + 1, &in_set, &out_set, &exc_set, &null_time ) < 0 )
   {
      perror( "i3_loop: select: Unable to poll I3_socket!" );
      i3_close_connection( TRUE );
      return;
   }

   if( FD_ISSET( I3_socket, &exc_set ) )
   {
      FD_CLR( I3_socket, &in_set );
      FD_CLR( I3_socket, &out_set );
      i3_log( "%s", "Exception raised on socket." );
      i3_close_connection( TRUE );
      return;
   }

   if( FD_ISSET( I3_socket, &in_set ) )
   {
      space_left = IPS - 4 - ((size_t)I3_input_pointer - (size_t)I3_input_buffer);
      ret = read( I3_socket, I3_input_buffer + I3_input_pointer, space_left );
      if( !ret || ( ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK ) )
      {
         FD_CLR( I3_socket, &out_set );
         if( ret < 0 )
            i3_log( "%s", "Read error on socket." );
         else
            i3_log( "%s", "EOF encountered on socket read." );
         i3_close_connection( TRUE );
         return;
      }
      if( ret < 0 )  /* EAGAIN */
         return;
      if( (size_t)ret == space_left )
          i3_log( "%s", "String overflow in I3 socket read!" );

      I3_input_pointer += ret;
      bytes_received += ret;
      if( packetdebug )
         i3_log( "Bytes received: %d", ret );
   }

   memcpy( &size, I3_input_buffer, 4 );
   size = ntohl( size );

   if( size <= I3_input_pointer - 4 )
   {
      I3_read_packet(  );
      I3_parse_packet(  );
   }
   return;
}

/*****************************************
 * User level commands and social hooks. *
 *****************************************/

/* This is very possibly going to be spammy as hell */
I3_CMD( i3_show_ucache_contents )
{
   UCACHE_DATA *user;
   int users = 0;

   i3_send_to_pager( "&wCached user information\r\n", ch );
   i3_send_to_pager( "&wUser                          | Gender ( 0 = Male, 1 = Female, 2 = Neuter )\r\n", ch );
   i3_send_to_pager( "&w---------------------------------------------------------------------------\r\n", ch );
   for( user = first_ucache; user; user = user->next )
   {
      i3_page_printf( ch, "&w%-30s %d\r\n", user->name, user->gender );
      users++;
   }
   i3_page_printf( ch, "&w%d users being cached.\r\n", users );
   return;
}

I3_CMD( i3_beep )
{
   char *ps;
   char mud[SMST];
   I3_MUD *pmud;
   char atbuf[SMST];

   if( I3IS_SET( I3FLAG( ch ), I3_DENYBEEP ) )
   {
      i3_to_char( "You are not allowed to use i3beeps.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3beep user@mud\r\n", ch );
      i3_to_char( "&wUsage: i3beep [on]/[off]\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "on" ) )
   {
      I3REMOVE_BIT( I3FLAG( ch ), I3_BEEP );
      i3_to_char( "You now send and receive i3beeps.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "off" ) )
   {
      I3SET_BIT( I3FLAG( ch ), I3_BEEP );
      i3_to_char( "You no longer send and receive i3beeps.\r\n", ch );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_BEEP ) )
   {
      i3_to_char( "Your i3beeps are turned off.\r\n", ch );
      return;
   }

   if( I3ISINVIS( ch ) )
   {
      i3_to_char( "You are invisible.\r\n", ch );
      return;
   }

   i3_strlcpy(atbuf, argument, SMST);
   ps = strchr( atbuf, '@' );

   if( !argument || argument[0] == '\0' || ps == NULL )
   {
      i3_to_char( "&YYou should specify a person@mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   ps[0] = '\0';
   ps++;
   i3_strlcpy( mud, ps, SMST );

   if( !( pmud = find_I3_mud_by_name( mud ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( !strcasecmp( I3_THISMUD, pmud->name ) )
   {
      i3_to_char( "Use your mud's own internal system for that.\r\n", ch );
      return;
   }

   if( pmud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", pmud->name );
      return;
   }

   if( pmud->beep == 0 )
      i3_printf( ch, "%s does not support the 'beep' command. Sending anyway.\r\n", pmud->name );

   i3_send_beep( ch, argument, pmud );
   i3_printf( ch, "&YYou i3beep %s@%s.\r\n", i3_capitalize( argument ), pmud->name );
}

I3_CMD( i3_tell )
{
   char to[SMST], *ps;
   char mud[SMST];
   I3_MUD *pmud;

   if( I3IS_SET( I3FLAG( ch ), I3_DENYTELL ) )
   {
      i3_to_char( "You are not allowed to use i3tells.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      int x;

      i3_to_char( "&wUsage: i3tell <user@mud> <message>\r\n", ch );
      i3_to_char( "&wUsage: i3tell [on]/[off]\r\n\r\n", ch );
      i3_printf( ch, "&cThe last %d things you were told over I3:\r\n", MAX_I3TELLHISTORY );

      for( x = 0; x < MAX_I3TELLHISTORY; x++ )
      {
         if( I3TELLHISTORY( ch, x ) == NULL )
            break;
         i3_printf( ch, "%s\r\n", I3TELLHISTORY( ch, x ) );
      }
      return;
   }

   if( !strcasecmp( argument, "on" ) )
   {
      I3REMOVE_BIT( I3FLAG( ch ), I3_TELL );
      i3_to_char( "You now send and receive i3tells.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "off" ) )
   {
      I3SET_BIT( I3FLAG( ch ), I3_TELL );
      i3_to_char( "You no longer send and receive i3tells.\r\n", ch );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_TELL ) )
   {
      i3_to_char( "Your i3tells are turned off.\r\n", ch );
      return;
   }

   if( I3ISINVIS( ch ) )
   {
      i3_to_char( "You are invisible.\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, to );
   ps = strchr( to, '@' );

   if( to[0] == '\0' || argument[0] == '\0' || ps == NULL )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   ps[0] = '\0';
   ps++;
   i3_strlcpy( mud, ps, SMST );

   if( !( pmud = find_I3_mud_by_name( mud ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( !strcasecmp( I3_THISMUD, pmud->name ) )
   {
      i3_to_char( "Use your mud's own internal system for that.\r\n", ch );
      return;
   }

   if( pmud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", pmud->name );
      return;
   }

   if( pmud->tell == 0 )
   {
      i3_printf( ch, "%s does not support the 'tell' command.\r\n", pmud->name );
      return;
   }

   i3_send_tell( ch, to, pmud, argument );
   snprintf( mud, SMST, "&YYou i3tell %s@%s: &c%s", i3_capitalize( to ), pmud->name, argument );
   i3_printf( ch, "%s\r\n", mud );
   i3_update_tellhistory( ch, mud );
}

I3_CMD( i3_reply )
{
   char buf[LGST];

   if( I3IS_SET( I3FLAG( ch ), I3_DENYTELL ) )
   {
      i3_to_char( "You are not allowed to use i3tells.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3reply <message>\r\n", ch );
      return;
   }

   if( I3IS_SET( I3FLAG( ch ), I3_TELL ) )
   {
      i3_to_char( "Your i3tells are turned off.\r\n", ch );
      return;
   }

   if( I3ISINVIS( ch ) )
   {
      i3_to_char( "You are invisible.\r\n", ch );
      return;
   }

   if( !I3REPLY( ch ) )
   {
      i3_to_char( "You have not yet received an i3tell?!?\r\n", ch );
      return;
   }

   snprintf( buf, LGST, "%s %s", I3REPLY( ch ), argument );
   i3_tell( ch, buf );
   return;
}

I3_CMD( i3_mudlisten )
{
   I3_CHANNEL *channel;
   char arg[SMST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3mudlisten [all/none]\r\n", ch );
      i3_to_char( "&wUsage: i3mudlisten <localchannel> [on/off]\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "all" ) )
   {
      for( channel = first_I3chan; channel; channel = channel->next )
      {
         if( !channel->local_name || channel->local_name[0] == '\0' )
            continue;

         i3_printf( ch, "Subscribing to %s.\r\n", channel->local_name );
         i3_send_channel_listen( channel, TRUE );
      }
      i3_to_char( "&YThe mud is now subscribed to all available local I3 channels.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "none" ) )
   {
      for( channel = first_I3chan; channel; channel = channel->next )
      {
         if( !channel->local_name || channel->local_name[0] == '\0' )
            continue;

         i3_printf( ch, "Unsubscribing from %s.\r\n", channel->local_name );
         i3_send_channel_listen( channel, FALSE );
      }
      i3_to_char( "&YThe mud is now unsubscribed from all available local I3 channels.\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, arg );
   if( !( channel = find_I3_channel_by_localname( arg ) ) )
   {
      i3_to_char( "No such channel configured locally.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "on" ) )
   {
      i3_printf( ch, "Turning %s channel on.\r\n", channel->local_name );
      i3_send_channel_listen( channel, TRUE );
      return;
   }

   if( !strcasecmp( argument, "off" ) )
   {
      i3_printf( ch, "Turning %s channel off.\r\n", channel->local_name );
      i3_send_channel_listen( channel, FALSE );
      return;
   }
   *arg = '\0';
   i3_mudlisten( ch, arg );
   return;
}

I3_CMD( i3_mudlist )
{
   I3_MUD *mud;
   char filter[SMST];
   int mudcount = 0;
   bool all = FALSE;

   *filter = '\0';
   argument = i3_one_argument( argument, filter );

   if( !strcasecmp( filter, "all" ) )
   {
      all = TRUE;
      argument = i3_one_argument( argument, filter );
      i3_send_to_pager( "&cShowing ALL known channels.\r\n\r\n", ch );
   }

   if( first_mud == NULL )
   {
      i3_to_char( "There are no muds to list!?\r\n", ch );
      return;
   }

   i3_page_printf( ch, "&W%-30s%-10.10s%-25.25s%-15.15s %s\r\n", "Name", "Type", "Mudlib", "Address", "Port" );
   for( mud = first_mud; mud; mud = mud->next )
   {
      if( mud == NULL )
      {
         i3_bug( "%s", "i3_mudlist: NULL mud found in listing!" );
         continue;
      }

      if( mud->name == NULL )
      {
         i3_bug( "%s", "i3_mudlist: NULL mud name found in listing!" );
         continue;
      }

      if( filter[0] && i3_str_prefix( filter, mud->name ) &&
          ( mud->mud_type && i3_str_prefix( filter, mud->mud_type ) ) &&
          ( mud->mudlib && i3_str_prefix( filter, mud->mudlib ) ) )
         continue;

      if( !all && mud->status == 0 )
         continue;

      mudcount++;

      switch ( mud->status )
      {
         case -1:
            i3_page_printf( ch, "&c%-30s%-10.10s%-25.25s%-15.15s %d\r\n",
                            mud->name, mud->mud_type, mud->mudlib, mud->ipaddress, mud->player_port );
            break;
         case 0:
            i3_page_printf( ch, "&R%-26s(down)\r\n", mud->name );
            break;
         default:
            i3_page_printf( ch, "&Y%-26s(rebooting, back in %d seconds)\r\n", mud->name, mud->status );
            break;
      }
   }
   i3_page_printf( ch, "&W%d total muds listed.\r\n", mudcount );
   return;
}

I3_CMD( i3_chanlist )
{
   I3_CHANNEL *channel;
   bool all = FALSE, found = FALSE;
   char filter[SMST];

   argument = i3_one_argument( argument, filter );

   if( !strcasecmp( filter, "all" ) && i3_is_connected(  ) )
   {
      all = TRUE;
      argument = i3_one_argument( argument, filter );
   }

   i3_send_to_pager( "&cLocal name          Perm    I3 Name             Hosted at           Status\r\n", ch );
   i3_send_to_pager( "&c-------------------------------------------------------------------------------\r\n", ch );
   for( channel = first_I3chan; channel; channel = channel->next )
   {
      found = FALSE;

      if( !all && !channel->local_name && ( filter[0] == '\0' ) )
         continue;

      if( I3PERM( ch ) < I3PERM_ADMIN && !channel->local_name )
         continue;

      if( I3PERM( ch ) < channel->i3perm )
         continue;

      if( !all && filter[0] != '\0' && i3_str_prefix( filter, channel->I3_name )
          && i3_str_prefix( filter, channel->host_mud ) )
         continue;

      if( channel->local_name && i3_has_name( I3LISTEN( ch ), channel->local_name ) )
         found = TRUE;

      i3_page_printf( ch, "&C%c &W%-18s&Y%-8s&B%-20s&P%-20s%-8s\r\n",
                      found ? '*' : ' ',
                      channel->local_name ? channel->local_name : "Not configured",
                      perm_names[channel->i3perm], channel->I3_name, channel->host_mud,
                      channel->status == 0 ? "&GPublic" : "&RPrivate" );
   }
   i3_send_to_pager( "&C*: You are listening to these channels.\r\n", ch );
   return;
}

I3_CMD( i3_setup_channel )
{
   DESCRIPTOR_DATA *d;
   CHAR_DATA *vch;
   char localname[SMST], I3_name[SMST];
   I3_CHANNEL *channel, *channel2;
   int permvalue = I3PERM_MORT;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3setchan <i3channelname> <localname> [permission]\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, I3_name );
   argument = i3_one_argument( argument, localname );

   if( !( channel = find_I3_channel_by_name( I3_name ) ) )
   {
      i3_to_char( "&YUnknown channel\r\n" "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   if( localname[0] == '\0' )
   {
      if( !channel->local_name )
      {
         i3_printf( ch, "Channel %s@%s isn't configured.\r\n", channel->I3_name, channel->host_mud );
         return;
      }

      if( channel->i3perm > I3PERM( ch ) )
      {
         i3_printf( ch, "You do not have sufficient permission to remove the %s channel.\r\n", channel->local_name );
         return;
      }

      for( d = first_descriptor; d; d = d->next )
      {
         vch = d->original ? d->original : d->character;

         if( !vch )
            continue;

         if( i3_has_name( I3LISTEN( vch ), channel->local_name ) )
            i3_unflagchan( &I3LISTEN( vch ), channel->local_name );
         if( i3_has_name( I3DENY( vch ), channel->local_name ) )
            i3_unflagchan( &I3DENY( vch ), channel->local_name );
      }
      i3_log( "setup_channel: removing %s as %s@%s", channel->local_name, channel->I3_name, channel->host_mud );
      i3_send_channel_listen( channel, FALSE );
      I3STRFREE( channel->local_name );
      i3_write_channel_config(  );
   }
   else
   {
      if( channel->local_name )
      {
         i3_printf( ch, "Channel %s@%s is already known as %s.\r\n", channel->I3_name, channel->host_mud,
                    channel->local_name );
         return;
      }
      if( ( channel2 = find_I3_channel_by_localname( localname ) ) )
      {
         i3_printf( ch, "Channel %s@%s is already known as %s.\r\n", channel2->I3_name, channel2->host_mud,
                    channel2->local_name );
         return;
      }

      if( argument && argument[0] != '\0' )
      {
         permvalue = get_permvalue( argument );
         if( permvalue < 0 || permvalue > I3PERM_IMP )
         {
            i3_to_char( "Invalid permission setting.\r\n", ch );
            return;
         }
         if( permvalue > I3PERM( ch ) )
         {
            i3_to_char( "You cannot assign a permission value above your own.\r\n", ch );
            return;
         }
      }
      channel->local_name = I3STRALLOC( localname );
      channel->i3perm = permvalue;
      I3STRFREE( channel->layout_m );
      I3STRFREE( channel->layout_e );
      channel->layout_m = I3STRALLOC( "&R[&W%s&R] &C%s@%s: &c%s" );
      channel->layout_e = I3STRALLOC( "&R[&W%s&R] &c%s" );
      i3_printf( ch, "%s@%s is now locally known as %s\r\n", channel->I3_name, channel->host_mud, channel->local_name );
      i3_log( "setup_channel: setting up %s@%s as %s", channel->I3_name, channel->host_mud, channel->local_name );
      i3_send_channel_listen( channel, TRUE );
      i3_write_channel_config(  );
   }
}

I3_CMD( i3_edit_channel )
{
   char localname[SMST];
   char arg2[SMST];
   I3_CHANNEL *channel;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3 editchan <localname> localname <new localname>\r\n", ch );
      i3_to_char( "&wUsage: i3 editchan <localname> perm <type>\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, localname );

   if( ( channel = find_I3_channel_by_localname( localname ) ) == NULL )
   {
      i3_to_char( "&YUnknown local channel\r\n"
                  "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, arg2 );

   if( channel->i3perm > I3PERM( ch ) )
   {
      i3_to_char( "You do not have sufficient permissions to edit this channel.\r\n", ch );
      return;
   }

   if( !strcasecmp( arg2, "localname" ) )
   {
      i3_printf( ch, "Local channel %s renamed to %s.\r\n", channel->local_name, argument );
      I3STRFREE( channel->local_name );
      channel->local_name = I3STRALLOC( argument );
      i3_write_channel_config(  );
      return;
   }

   if( !strcasecmp( arg2, "perm" ) || !strcasecmp( arg2, "permission" ) )
   {
      int permvalue = get_permvalue( argument );

      if( permvalue < 0 || permvalue > I3PERM_IMP )
      {
         i3_to_char( "Invalid permission setting.\r\n", ch );
         return;
      }
      if( permvalue > I3PERM( ch ) )
      {
         i3_to_char( "You cannot set a permission higher than your own.\r\n", ch );
         return;
      }
      if( channel->i3perm > I3PERM( ch ) )
      {
         i3_to_char( "You cannot edit a channel above your permission level.\r\n", ch );
         return;
      }
      channel->i3perm = permvalue;
      i3_printf( ch, "Local channel %s permission changed to %s.\r\n", channel->local_name, argument );
      i3_write_channel_config(  );
      return;
   }
   *arg2 = '\0';
   i3_edit_channel( ch, arg2 );
   return;
}

I3_CMD( i3_chan_who )
{
   char channel_name[SMST];
   I3_CHANNEL *channel;
   I3_MUD *mud;

   argument = i3_one_argument( argument, channel_name );

   if( channel_name[0] == '\0' || !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3chanwho <local channel> <mud>\r\n", ch );
      return;
   }

   if( ( channel = find_I3_channel_by_localname( channel_name ) ) == NULL )
   {
      i3_to_char( "&YUnknown channel.\r\n" "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   if( !( mud = find_I3_mud_by_name( argument ) ) )
   {
      i3_to_char( "&YUnknown mud.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( mud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", mud->name );
      return;
   }

   I3_send_chan_who( ch, channel, mud );
}

I3_CMD( i3_listen_channel )
{
   I3_CHANNEL *channel;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&cCurrently tuned into:\r\n", ch );
      i3_printf( ch, "&W%s\r\n", ( I3LISTEN( ch ) && I3LISTEN( ch )[0] != '\0' ) ? I3LISTEN( ch ) : "None" );
      return;
   }

   if( !strcasecmp( argument, "all" ) )
   {
      for( channel = first_I3chan; channel; channel = channel->next )
      {
         if( !channel->local_name || channel->local_name[0] == '\0' )
            continue;

         if( I3PERM( ch ) >= channel->i3perm && !i3_has_name( I3LISTEN( ch ), channel->local_name ) )
            i3_flagchan( &I3LISTEN( ch ), channel->local_name );
      }
      i3_to_char( "&YYou are now listening to all available I3 channels.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "none" ) )
   {
      for( channel = first_I3chan; channel; channel = channel->next )
      {
         if( !channel->local_name || channel->local_name[0] == '\0' )
            continue;

         if( i3_has_name( I3LISTEN( ch ), channel->local_name ) )
            i3_unflagchan( &I3LISTEN( ch ), channel->local_name );
      }
      i3_to_char( "&YYou no longer listen to any available I3 channels.\r\n", ch );
      return;
   }

   if( ( channel = find_I3_channel_by_localname( argument ) ) == NULL )
   {
      i3_to_char( "&YUnknown channel.\r\n" "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   if( i3_has_name( I3LISTEN( ch ), channel->local_name ) )
   {
      i3_printf( ch, "You no longer listen to %s\r\n", channel->local_name );
      i3_unflagchan( &I3LISTEN( ch ), channel->local_name );
   }
   else
   {
      if( I3PERM( ch ) < channel->i3perm )
      {
         i3_printf( ch, "Channel %s is above your permission level.\r\n", channel->local_name );
         return;
      }
      i3_printf( ch, "You now listen to %s\r\n", channel->local_name );
      i3_flagchan( &I3LISTEN( ch ), channel->local_name );
   }
   return;
}

I3_CMD( i3_deny_channel )
{
   char vic_name[SMST];
   CHAR_DATA *victim;
   I3_CHANNEL *channel;

   argument = i3_one_argument( argument, vic_name );

   if( vic_name[0] == '\0' || !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3deny <person> <local channel name>\r\n", ch );
      i3_to_char( "&wUsage: i3deny <person> [tell/beep/finger]\r\n", ch );
      return;
   }

   if( !( victim = i3_find_user( vic_name ) ) )
   {
      i3_to_char( "No such person is currently online.\r\n", ch );
      return;
   }

   if( I3PERM( ch ) <= I3PERM( victim ) )
   {
      i3_to_char( "You cannot alter their settings.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "tell" ) )
   {
      if( !I3IS_SET( I3FLAG( victim ), I3_DENYTELL ) )
      {
         I3SET_BIT( I3FLAG( victim ), I3_DENYTELL );
         i3_printf( ch, "%s can no longer use i3tells.\r\n", CH_I3NAME( victim ) );
         return;
      }
      I3REMOVE_BIT( I3FLAG( victim ), I3_DENYTELL );
      i3_printf( ch, "%s can use i3tells again.\r\n", CH_I3NAME( victim ) );
      return;
   }

   if( !strcasecmp( argument, "beep" ) )
   {
      if( !I3IS_SET( I3FLAG( victim ), I3_DENYBEEP ) )
      {
         I3SET_BIT( I3FLAG( victim ), I3_DENYBEEP );
         i3_printf( ch, "%s can no longer use i3beeps.\r\n", CH_I3NAME( victim ) );
         return;
      }
      I3REMOVE_BIT( I3FLAG( victim ), I3_DENYBEEP );
      i3_printf( ch, "%s can use i3beeps again.\r\n", CH_I3NAME( victim ) );
      return;
   }

   if( !strcasecmp( argument, "finger" ) )
   {
      if( !I3IS_SET( I3FLAG( victim ), I3_DENYFINGER ) )
      {
         I3SET_BIT( I3FLAG( victim ), I3_DENYFINGER );
         i3_printf( ch, "%s can no longer use i3fingers.\r\n", CH_I3NAME( victim ) );
         return;
      }
      I3REMOVE_BIT( I3FLAG( victim ), I3_DENYFINGER );
      i3_printf( ch, "%s can use i3fingers again.\r\n", CH_I3NAME( victim ) );
      return;
   }

   if( !( channel = find_I3_channel_by_localname( argument ) ) )
   {
      i3_to_char( "&YUnknown channel.\r\n" "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   if( i3_has_name( I3DENY( ch ), channel->local_name ) )
   {
      i3_printf( ch, "%s can now listen to %s\r\n", CH_I3NAME( victim ), channel->local_name );
      i3_unflagchan( &I3DENY( ch ), channel->local_name );
   }
   else
   {
      i3_printf( ch, "%s can no longer listen to %s\r\n", CH_I3NAME( victim ), channel->local_name );
      i3_flagchan( &I3DENY( ch ), channel->local_name );
   }
   return;
}

I3_CMD( i3_mudinfo )
{
   I3_MUD *mud;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3mudinfo <mudname>\r\n", ch );
      return;
   }

   if( !( mud = find_I3_mud_by_name( argument ) ) )
   {
      i3_to_char( "&YUnknown mud.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   i3_printf( ch, "&WInformation about %s\r\n\r\n", mud->name );
   if( mud->status == 0 )
      i3_to_char( "&wStatus     : Currently down\r\n", ch );
   else if( mud->status > 0 )
      i3_printf( ch, "&wStatus     : Currently rebooting, back in %d seconds\r\n", mud->status );
   i3_printf( ch, "&wMUD port   : %s %d\r\n", mud->ipaddress, mud->player_port );
   i3_printf( ch, "&wBase mudlib: %s\r\n", mud->base_mudlib );
   i3_printf( ch, "&wMudlib     : %s\r\n", mud->mudlib );
   i3_printf( ch, "&wDriver     : %s\r\n", mud->driver );
   i3_printf( ch, "&wType       : %s\r\n", mud->mud_type );
   i3_printf( ch, "&wOpen status: %s\r\n", mud->open_status );
   i3_printf( ch, "&wAdmin      : %s\r\n", mud->admin_email );
   if( mud->web )
      i3_printf( ch, "&wURL        : %s\r\n", mud->web );
   if( mud->web_wrong && !mud->web )
      i3_printf( ch, "&wURL        : %s\r\n", mud->web_wrong );
   if( mud->daemon )
      i3_printf( ch, "&wDaemon     : %s\r\n", mud->daemon );
   if( mud->time )
      i3_printf( ch, "&wTime       : %s\r\n", mud->time );
   if( mud->banner )
      i3_printf( ch, "&wBanner:\r\n%s\r\n", mud->banner );

   i3_to_char( "&wSupports   : ", ch );
   if( mud->tell )
      i3_to_char( "&wtell, ", ch );
   if( mud->beep )
      i3_to_char( "&wbeep, ", ch );
   if( mud->emoteto )
      i3_to_char( "&wemoteto, ", ch );
   if( mud->who )
      i3_to_char( "&wwho, ", ch );
   if( mud->finger )
      i3_to_char( "&wfinger, ", ch );
   if( mud->locate )
      i3_to_char( "&wlocate, ", ch );
   if( mud->channel )
      i3_to_char( "&wchannel, ", ch );
   if( mud->news )
      i3_to_char( "&wnews, ", ch );
   if( mud->mail )
      i3_to_char( "&wmail, ", ch );
   if( mud->file )
      i3_to_char( "&wfile, ", ch );
   if( mud->auth )
      i3_to_char( "&wauth, ", ch );
   if( mud->ucache )
      i3_to_char( "&wucache, ", ch );
   i3_to_char( "\r\n", ch );

   i3_to_char( "&wSupports   : ", ch );
   if( mud->smtp )
      i3_printf( ch, "&wsmtp (port %d), ", mud->smtp );
   if( mud->http )
      i3_printf( ch, "&whttp (port %d), ", mud->http );
   if( mud->ftp )
      i3_printf( ch, "&wftp  (port %d), ", mud->ftp );
   if( mud->pop3 )
      i3_printf( ch, "&wpop3 (port %d), ", mud->pop3 );
   if( mud->nntp )
      i3_printf( ch, "&wnntp (port %d), ", mud->nntp );
   if( mud->rcp )
      i3_printf( ch, "&wrcp  (port %d), ", mud->rcp );
   if( mud->amrcp )
      i3_printf( ch, "&wamrcp (port %d), ", mud->amrcp );
   i3_to_char( "\r\n", ch );
}

I3_CMD( i3_chanlayout )
{
   I3_CHANNEL *channel = NULL;
   char arg1[SMST], arg2[SMST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3chanlayout <localchannel|all> <layout> <format...>\r\n", ch );
      i3_to_char( "&wLayout can be one of these: layout_e layout_m\r\n", ch );
      i3_to_char( "&wFormat can be any way you want it to look, provided you have the proper number of %s tags in it.\r\n",
                  ch );
      return;
   }

   argument = i3_one_argument( argument, arg1 );
   argument = i3_one_argument( argument, arg2 );

   if( arg1[0] == '\0' )
   {
      i3_chanlayout( ch, arg1 );
      return;
   }
   if( arg2[0] == '\0' )
   {
      i3_chanlayout( ch, arg2 );
      return;
   }
   if( !argument || argument[0] == '\0' )
   {
      *arg1 = '\0';
      i3_chanlayout( ch, arg1 );
      return;
   }

   if( !( channel = find_I3_channel_by_localname( arg1 ) ) )
   {
      i3_to_char( "&YUnknown channel.\r\n" "(use &Wi3chanlist&Y to get an overview of the channels available)\r\n", ch );
      return;
   }

   if( !strcasecmp( arg2, "layout_e" ) )
   {
      if( !i3_verify_layout( argument, 2 ) )
      {
         i3_to_char( "Incorrect format for layout_e. You need exactly 2 %s's.\r\n", ch );
         return;
      }
      I3STRFREE( channel->layout_e );
      channel->layout_e = I3STRALLOC( argument );
      i3_to_char( "Channel layout_e changed.\r\n", ch );
      i3_write_channel_config(  );
      return;
   }

   if( !strcasecmp( arg2, "layout_m" ) )
   {
      if( !i3_verify_layout( argument, 4 ) )
      {
         i3_to_char( "Incorrect format for layout_m. You need exactly 4 %s's.\r\n", ch );
         return;
      }
      I3STRFREE( channel->layout_m );
      channel->layout_m = I3STRALLOC( argument );
      i3_to_char( "Channel layout_m changed.\r\n", ch );
      i3_write_channel_config(  );
      return;
   }
   *arg1 = '\0';
   i3_chanlayout( ch, arg1 );
   return;
}

I3_CMD( i3_bancmd )
{
   I3_BAN *temp;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&GThe mud currently has the following ban list:\r\n\r\n", ch );

      if( !first_i3ban )
         i3_to_char( "&YNothing\r\n", ch );
      else
      {
         for( temp = first_i3ban; temp; temp = temp->next )
            i3_printf( ch, "&Y\t  - %s\r\n", temp->name );
      }
      i3_to_char( "\r\n&YTo add a ban, just specify a target. Suggested targets being user@mud or IP:Port\r\n", ch );
      i3_to_char( "&YUser@mud bans can also have wildcard specifiers, such as *@Mud or User@*\r\n", ch );
      return;
   }

   if( !fnmatch( argument, I3_THISMUD, 0 ) )
   {
      i3_to_char( "&YYou don't really want to do that....\r\n", ch );
      return;
   }

   for( temp = first_i3ban; temp; temp = temp->next )
   {
      if( !strcasecmp( temp->name, argument ) )
      {
         I3STRFREE( temp->name );
         I3UNLINK( temp, first_i3ban, last_i3ban, next, prev );
         I3DISPOSE( temp );
         I3_write_bans(  );
         i3_printf( ch, "&YThe mud no longer bans %s.\r\n", argument );
         return;
      }
   }
   I3CREATE( temp, I3_BAN, 1 );
   temp->name = I3STRALLOC( argument );
   I3LINK( temp, first_i3ban, last_i3ban, next, prev );
   I3_write_bans(  );
   i3_printf( ch, "&YThe mud now bans all incoming traffic from %s.\r\n", temp->name );
}

I3_CMD( i3_ignorecmd )
{
   I3_IGNORE *temp;
   char buf[SMST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&GYou are currently ignoring the following:\r\n\r\n", ch );

      if( !FIRST_I3IGNORE( ch ) )
      {
         i3_to_char( "&YNobody\r\n\r\n", ch );
         i3_to_char( "&YTo add an ignore, just specify a target. Suggested targets being user@mud or IP:Port\r\n", ch );
         i3_to_char( "&YUser@mud ignores can also have wildcard specifiers, such as *@Mud or User@*\r\n", ch );
         return;
      }
      for( temp = FIRST_I3IGNORE( ch ); temp; temp = temp->next )
         i3_printf( ch, "&Y\t  - %s\r\n", temp->name );

      return;
   }

   snprintf( buf, SMST, "%s@%s", CH_I3NAME( ch ), I3_THISMUD );
   if( !strcasecmp( buf, argument ) )
   {
      i3_to_char( "&YYou don't really want to do that....\r\n", ch );
      return;
   }

   if( !fnmatch( argument, I3_THISMUD, 0 ) )
   {
      i3_to_char( "&YIgnoring your own mud would be silly.\r\n", ch );
      return;
   }

   for( temp = FIRST_I3IGNORE( ch ); temp; temp = temp->next )
   {
      if( !strcasecmp( temp->name, argument ) )
      {
         I3STRFREE( temp->name );
         I3UNLINK( temp, FIRST_I3IGNORE( ch ), LAST_I3IGNORE( ch ), next, prev );
         I3DISPOSE( temp );
         i3_printf( ch, "&YYou are no longer ignoring %s.\r\n", argument );
         return;
      }
   }

   I3CREATE( temp, I3_IGNORE, 1 );
   temp->name = I3STRALLOC( argument );
   I3LINK( temp, FIRST_I3IGNORE( ch ), LAST_I3IGNORE( ch ), next, prev );
   i3_printf( ch, "&YYou now ignore %s.\r\n", temp->name );
}

I3_CMD( i3_invis )
{
   if( I3INVIS( ch ) )
   {
      I3REMOVE_BIT( I3FLAG( ch ), I3_INVIS );
      i3_to_char( "You are now i3visible.\r\n", ch );
   }
   else
   {
      I3SET_BIT( I3FLAG( ch ), I3_INVIS );
      i3_to_char( "You are now i3invisible.\r\n", ch );
   }
   return;
}

I3_CMD( i3_debug )
{
   packetdebug = !packetdebug;

   if( packetdebug )
      i3_to_char( "Packet debugging enabled.\r\n", ch );
   else
      i3_to_char( "Packet debugging disabled.\r\n", ch );

   return;
}

I3_CMD( i3_send_user_req )
{
   char user[SMST], mud[SMST];
   char *ps;
   I3_MUD *pmud;
   char atbuf[SMST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&YQuery who at which mud?\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }
   i3_strlcpy(atbuf, argument, SMST);
   if( !( ps = strchr( atbuf, '@' ) ) )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   ps[0] = '\0';
   i3_strlcpy( user, argument, SMST );
   i3_strlcpy( mud, ps + 1, SMST );

   if( user[0] == '\0' || mud[0] == '\0' )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( !( pmud = find_I3_mud_by_name( mud ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( pmud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", pmud->name );
      return;
   }

   I3_send_chan_user_req( pmud->name, user );
   return;
}

I3_CMD( i3_admin_channel )
{
   I3_CHANNEL *channel = NULL;
   char arg1[SMST], arg2[SMST], buf[LGST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3adminchan <localchannel> <add|remove> <mudname>\r\n", ch );
      i3_to_char( "&wUsage: i3adminchan <localchannel> list\r\n", ch );
      return;
   }
   argument = i3_one_argument( argument, arg1 );
   argument = i3_one_argument( argument, arg2 );

   if( arg1[0] == '\0' )
   {
      i3_admin_channel( ch, arg1 );
      return;
   }

   if( !( channel = find_I3_channel_by_localname( arg1 ) ) )
   {
      i3_to_char( "No such channel with that name here.\r\n", ch );
      return;
   }

   if( arg2[0] == '\0' )
   {
      i3_admin_channel( ch, arg2 );
      return;
   }

   if( !strcasecmp( arg2, "list" ) )
   {
      I3_send_channel_adminlist( ch, channel->I3_name );
      i3_to_char( "Sending request for administrative list.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      *arg1 = '\0';
      i3_admin_channel( ch, arg1 );
      return;
   }

   if( !strcasecmp( arg2, "add" ) )
   {
      snprintf( buf, LGST, "({\"%s\",}),({}),", argument );
      I3_send_channel_admin( ch, channel->I3_name, buf );
      i3_to_char( "Sending administrative list addition.\r\n", ch );
      return;
   }

   if( !strcasecmp( arg2, "remove" ) )
   {
      snprintf( buf, LGST, "({}),({\"%s\",}),", argument );
      I3_send_channel_admin( ch, channel->I3_name, buf );
      i3_to_char( "Sending administrative list removal.\r\n", ch );
      return;
   }
   *arg1 = '\0';
   i3_admin_channel( ch, arg1 );
   return;
}

I3_CMD( i3_disconnect )
{
   if( !i3_is_connected(  ) )
   {
      i3_to_char( "The MUD isn't connected to the Intermud-3 router.\r\n", ch );
      return;
   }

   i3_to_char( "Disconnecting from Intermud-3 router.\r\n", ch );

   i3_shutdown( 0 );
   return;
}

I3_CMD( i3_connect )
{
   ROUTER_DATA *router;

   if( i3_is_connected(  ) )
   {
      i3_printf( ch, "The MUD is already connected to Intermud-3 router %s\r\n", I3_ROUTER_NAME );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Connecting to Intermud-3\r\n", ch );
      router_connect( NULL, TRUE, this_i3mud->player_port, FALSE );
      return;
   }

   for( router = first_router; router; router = router->next )
   {
      if( !strcasecmp( router->name, argument ) )
      {
         router->reconattempts = 0;
         i3_printf( ch, "Connecting to Intermud-3 router %s\r\n", argument );
         router_connect( argument, TRUE, this_i3mud->player_port, FALSE );
         return;
      }
   }

   i3_printf( ch, "%s is not configured as a router for this mud.\r\n", argument );
   i3_to_char( "If you wish to add it, use the i3router command to provide its information.\r\n", ch );
   return;
}

I3_CMD( i3_reload )
{
    int mudport = this_i3mud->player_port;

    if(i3_is_connected()) {
        i3_to_char( "Disconnecting from I3 router...\r\n", ch );
        i3_shutdown(0);
    }
    i3_to_char( "Reloading I3 configuration...\r\n", ch );
    if(i3_read_config(mudport)) {
        i3_to_char( "Reconnecting to I3 router...\r\n", ch );
        router_connect( NULL, FALSE, this_i3mud->player_port, FALSE );
    }
    i3_to_char( "Done!\r\n", ch );
    return;
}

I3_CMD( i3_addchan )
{
   I3_CHANNEL *channel;
   char arg[SMST], arg2[SMST], buf[LGST];
   int type, x;

   argument = i3_one_argument( argument, arg );
   argument = i3_one_argument( argument, arg2 );

   if( !argument || argument[0] == '\0' || arg[0] == '\0' || arg2[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3addchan <channelname> <localname> <type>\r\n\r\n", ch );
      i3_to_char( "&wChannelname should be the name seen on 'chanlist all'\r\n", ch );
      i3_to_char( "&wLocalname should be the local name you want it listed as.\r\n", ch );
      i3_to_char( "&wType can be one of the following:\r\n\r\n", ch );
      i3_to_char( "&w0: selectively banned\r\n", ch );
      i3_to_char( "&w1: selectively admitted\r\n", ch );
      i3_to_char( "&w2: filtered - valid for selectively admitted ONLY\r\n", ch );
      return;
   }

   if( ( channel = find_I3_channel_by_name( arg ) ) != NULL )
   {
      i3_printf( ch, "&R%s is already hosted by %s.\r\n", channel->I3_name, channel->host_mud );
      return;
   }

   if( ( channel = find_I3_channel_by_localname( arg2 ) ) != NULL )
   {
      i3_printf( ch, "&RChannel %s@%s is already locally configured as %s.\r\n",
                 channel->I3_name, channel->host_mud, channel->local_name );
      return;
   }

   if( !isdigit( argument[0] ) )
   {
      i3_to_char( "&RInvalid type. Must be numerical.\r\n", ch );
      *arg = '\0';
      i3_addchan( ch, arg );
      return;
   }

   type = atoi( argument );
   if( type < 0 || type > 2 )
   {
      i3_to_char( "&RInvalid channel type.\r\n", ch );
      *arg = '\0';
      i3_addchan( ch, arg );
      return;
   }

   i3_printf( ch, "&GAdding channel to router: &W%s\r\n", arg );
   I3_send_channel_add( ch, arg, type );

   I3CREATE( channel, I3_CHANNEL, 1 );
   channel->I3_name = I3STRALLOC( arg );
   channel->host_mud = I3STRALLOC( I3_THISMUD );
   channel->local_name = I3STRALLOC( arg2 );
   channel->i3perm = I3PERM_ADMIN;
   channel->layout_m = I3STRALLOC( "&R[&W%s&R] &C%s@%s: &c%s" );
   channel->layout_e = I3STRALLOC( "&R[&W%s&R] &c%s" );
   for( x = 0; x < MAX_I3HISTORY; x++ )
      channel->history[x] = NULL;
   I3LINK( channel, first_I3chan, last_I3chan, next, prev );

   if( type != 0 )
   {
      snprintf( buf, LGST, "({\"%s\",}),({}),", I3_THISMUD );
      I3_send_channel_admin( ch, channel->I3_name, buf );
      i3_printf( ch, "&GSending command to add %s to the invite list.\r\n", I3_THISMUD );
   }

   i3_printf( ch, "&Y%s@%s &Wis now locally known as &Y%s\r\n", channel->I3_name, channel->host_mud, channel->local_name );
   i3_send_channel_listen( channel, TRUE );
   i3_write_channel_config(  );

   return;
}

I3_CMD( i3_removechan )
{
   I3_CHANNEL *channel = NULL;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3removechan <channel>\r\n", ch );
      i3_to_char( "&wChannelname should be the name seen on 'chanlist all'\r\n", ch );
      return;
   }

   if( ( channel = find_I3_channel_by_name( argument ) ) == NULL )
   {
      i3_to_char( "&RNo channel by that name exists.\r\n", ch );
      return;
   }

   if( strcasecmp( channel->host_mud, I3_THISMUD ) )
   {
      i3_printf( ch, "&R%s does not host this channel and cannot remove it.\r\n", I3_THISMUD );
      return;
   }

   i3_printf( ch, "&YRemoving channel from router: &W%s\r\n", channel->I3_name );
   I3_send_channel_remove( ch, channel );

   i3_printf( ch, "&RDestroying local channel entry for &W%s\r\n", channel->I3_name );
   destroy_I3_channel( channel );
   i3_write_channel_config(  );

   return;
}

I3_CMD( i3_setconfig )
{
   char arg[SMST];

   argument = i3_one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      i3_to_char( "&GConfiguration info for your mud. Changes save when edited.\r\n", ch );
      i3_to_char( "&GYou can set the following:\r\n\r\n", ch );
      i3_to_char( "&wShow       : &GDisplays your current congfiguration.\r\n", ch );
      i3_to_char( "&wAutoconnect: &GA toggle. Either on or off. Your mud will connect automatically with it on.\r\n", ch );
      i3_to_char( "&wMudname    : &GThe name you want displayed on I3 for your mud.\r\n", ch );
      i3_to_char( "&wTelnet     : &GThe telnet address for your mud. Do not include the port number.\r\n", ch );
      i3_to_char( "&wWeb        : &GThe website address for your mud. In the form of: www.address.com.\r\n", ch );
      i3_to_char( "&wEmail      : &GThe email address of your mud's administrator. Needs to be valid!!\r\n", ch );
      i3_to_char( "&wStatus     : &GThe open status of your mud. IE: Public, Development, etc.\r\n", ch );
      i3_to_char( "&wMudtype    : &GWhat type of mud you have. Diku, LP, CoffeeMUD, etc.\r\n", ch );
      i3_to_char( "&wBaselib    : &GThe base version of the codebase you have. DikuMUD Alfa, Merc, Smaug, etc.\r\n", ch );
      i3_to_char( "&wMudlib     : &GWhat you call the current version of your codebase.  Such as SmaugFUSS 1.23\r\n", ch );
      i3_to_char( "&wMinlevel   : &GMinimum level at which I3 will recognize your players.\r\n", ch );
      i3_to_char( "&wImmlevel   : &GThe level at which immortal commands become available.\r\n", ch );
      i3_to_char( "&wAdminlevel : &GThe level at which administrative commands become available.\r\n", ch );
      i3_to_char( "&wImplevel   : &GThe level at which implementor commands become available.\r\n", ch );
      return;
   }

   if( !strcasecmp( arg, "show" ) )
   {
      i3_printf( ch, "&wMudname       : &G%s\r\n", this_i3mud->name );
      i3_printf( ch, "&wAutoconnect   : &G%s\r\n", this_i3mud->autoconnect == TRUE ? "Enabled" : "Disabled" );
      i3_printf( ch, "&wTelnet        : &G%s:%d\r\n", this_i3mud->telnet, this_i3mud->player_port );
      i3_printf( ch, "&wWeb           : &G%s\r\n", this_i3mud->web );
      i3_printf( ch, "&wEmail         : &G%s\r\n", this_i3mud->admin_email );
      i3_printf( ch, "&wStatus        : &G%s\r\n", this_i3mud->open_status );
      i3_printf( ch, "&wMudtype       : &G%s\r\n", this_i3mud->mud_type );
      i3_printf( ch, "&wBaselib       : &G%s\r\n", this_i3mud->base_mudlib );
      i3_printf( ch, "&wMudlib        : &G%s\r\n", this_i3mud->mudlib );
      i3_printf( ch, "&wMinlevel      : &G%d\r\n", this_i3mud->minlevel );
      i3_printf( ch, "&wImmlevel      : &G%d\r\n", this_i3mud->immlevel );
      i3_printf( ch, "&wAdminlevel    : &G%d\r\n", this_i3mud->adminlevel );
      i3_printf( ch, "&wImplevel      : &G%d\r\n", this_i3mud->implevel );
      return;
   }

   if( !strcasecmp( arg, "autoconnect" ) )
   {
      this_i3mud->autoconnect = !this_i3mud->autoconnect;

      if( this_i3mud->autoconnect )
         i3_to_char( "Autoconnect enabled.\r\n", ch );
      else
         i3_to_char( "Autoconnect disabled.\r\n", ch );
      i3_save_config(  );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      *arg = '\0';
      i3_setconfig( ch, arg );
      return;
   }

   if( !strcasecmp( arg, "implevel" ) && I3PERM( ch ) == I3PERM_IMP )
   {
      int value = atoi( argument );

      this_i3mud->implevel = value;
      i3_save_config(  );
      i3_printf( ch, "Implementor level changed to %d\r\n", value );
      return;
   }

   if( !strcasecmp( arg, "adminlevel" ) )
   {
      int value = atoi( argument );

      this_i3mud->adminlevel = value;
      i3_save_config(  );
      i3_printf( ch, "Admin level changed to %d\r\n", value );
      return;
   }

   if( !strcasecmp( arg, "immlevel" ) )
   {
      int value = atoi( argument );

      this_i3mud->immlevel = value;
      i3_save_config(  );
      i3_printf( ch, "Immortal level changed to %d\r\n", value );
      return;
   }

   if( !strcasecmp( arg, "minlevel" ) )
   {
      int value = atoi( argument );

      this_i3mud->minlevel = value;
      i3_save_config(  );
      i3_printf( ch, "Minimum level changed to %d\r\n", value );
      return;
   }

   if( i3_is_connected(  ) )
   {
      i3_printf( ch, "%s may not be changed while the mud is connected.\r\n", arg );
      return;
   }

   if( !strcasecmp( arg, "mudname" ) )
   {
      I3STRFREE( this_i3mud->name );
      this_i3mud->name = I3STRALLOC( argument );
      //I3_THISMUD = argument;
      unlink( I3_PASSWORD_FILE );
      i3_save_config(  );
      i3_printf( ch, "Mud name changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "telnet" ) )
   {
      I3STRFREE( this_i3mud->telnet );
      this_i3mud->telnet = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Telnet address changed to %s:%d\r\n", argument, this_i3mud->player_port );
      return;
   }

   if( !strcasecmp( arg, "web" ) )
   {
      I3STRFREE( this_i3mud->web );
      this_i3mud->web = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Website changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "email" ) )
   {
      I3STRFREE( this_i3mud->admin_email );
      this_i3mud->admin_email = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Admin email changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "status" ) )
   {
      I3STRFREE( this_i3mud->open_status );
      this_i3mud->open_status = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Status changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "mudtype" ) )
   {
      I3STRFREE( this_i3mud->mud_type );
      this_i3mud->mud_type = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Mud type changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "baselib" ) )
   {
      I3STRFREE( this_i3mud->base_mudlib );
      this_i3mud->base_mudlib = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Base mudlib changed to %s\r\n", argument );
      return;
   }

   if( !strcasecmp( arg, "mudlib" ) )
   {
      I3STRFREE( this_i3mud->mudlib );
      this_i3mud->mudlib = I3STRALLOC( argument );
      i3_save_config(  );
      i3_printf( ch, "Mudlib changed to %s\r\n", argument );
      return;
   }

   *arg = '\0';
   i3_setconfig( ch, arg );
   return;
}

I3_CMD( i3_permstats )
{
   CHAR_DATA *victim;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3perms <user>\r\n", ch );
      return;
   }

   if( !( victim = i3_find_user( argument ) ) )
   {
      i3_to_char( "No such person is currently online.\r\n", ch );
      return;
   }

   if( I3PERM( victim ) < 0 || I3PERM( victim ) > I3PERM_IMP )
   {
      i3_printf( ch, "&R%s has an invalid permission setting!\r\n", CH_I3NAME( victim ) );
      return;
   }

   i3_printf( ch, "&GPermissions for %s: %s\r\n", CH_I3NAME( victim ), perm_names[I3PERM( victim )] );
   i3_printf( ch, "&gThese permissions were obtained %s.\r\n",
              I3IS_SET( I3FLAG( victim ), I3_PERMOVERRIDE ) ? "manually via i3permset" : "automatically by level" );
   return;
}

I3_CMD( i3_permset )
{
   CHAR_DATA *victim;
   char arg[SMST];
   int permvalue;

   argument = i3_one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3permset <user> <permission>\r\n", ch );
      i3_to_char( "&wPermission can be one of: None, Mort, Imm, Admin, Imp\r\n", ch );
      return;
   }

   if( !( victim = i3_find_user( arg ) ) )
   {
      i3_to_char( "No such person is currently online.\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "override" ) )
      permvalue = -1;
   else
   {
      permvalue = get_permvalue( argument );

      if( !i3check_permissions( ch, permvalue, I3PERM( victim ), TRUE ) )
         return;
   }

   /*
    * Just something to avoid looping through the channel clean-up --Xorith 
    */
   if( I3PERM( victim ) == permvalue )
   {
      i3_printf( ch, "%s already has a permission level of %s.\r\n", CH_I3NAME( victim ), perm_names[permvalue] );
      return;
   }

   if( permvalue == -1 )
   {
      I3REMOVE_BIT( I3FLAG( victim ), I3_PERMOVERRIDE );
      i3_printf( ch, "&YPermission flag override has been removed from %s\r\n", CH_I3NAME( victim ) );
      return;
   }

   I3PERM( victim ) = permvalue;
   I3SET_BIT( I3FLAG( victim ), I3_PERMOVERRIDE );

   i3_printf( ch, "&YPermission level for %s has been changed to %s\r\n", CH_I3NAME( victim ), perm_names[permvalue] );
   /*
    * Channel Clean-Up added by Xorith 9-24-03 
    */
   /*
    * Note: Let's not clean up I3_DENY for a player. Never know... 
    */
   if( I3LISTEN( victim ) != NULL )
   {
      I3_CHANNEL *channel = NULL;
      const char *channels = I3LISTEN( victim );

      while( 1 )
      {
         if( channels[0] == '\0' )
            break;
         channels = i3_one_argument( channels, arg );

         if( !( channel = find_I3_channel_by_localname( arg ) ) )
            i3_unflagchan( &I3LISTEN( victim ), arg );
         if( channel && I3PERM( victim ) < channel->i3perm )
         {
            i3_unflagchan( &I3LISTEN( victim ), arg );
            i3_printf( ch, "&WRemoving '%s' level channel: '%s', exceeding new permission of '%s'\r\n",
                       perm_names[channel->i3perm], channel->local_name, perm_names[I3PERM( victim )] );
         }
      }
   }
   return;
}

I3_CMD( i3_who )
{
   I3_MUD *mud;

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3who <mudname>\r\n", ch );
      return;
   }

   if( !( mud = find_I3_mud_by_name( argument ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( mud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", mud->name );
      return;
   }

   if( mud->who == 0 )
      i3_printf( ch, "%s does not support the 'who' command. Sending anyway.\r\n", mud->name );

   i3_send_who( ch, mud->name );
}

I3_CMD( i3_locate )
{
   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3locate <person>\r\n", ch );
      return;
   }
   i3_send_locate( ch, argument );
}

I3_CMD( i3_finger )
{
   char user[SMST], mud[SMST];
   char *ps;
   I3_MUD *pmud;
   char atbuf[SMST];

   if( I3IS_SET( I3FLAG( ch ), I3_DENYFINGER ) )
   {
      i3_to_char( "You are not allowed to use i3finger.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3finger <user@mud>\r\n", ch );
      i3_to_char( "&wUsage: i3finger privacy\r\n", ch );
      return;
   }

   if( !strcasecmp( argument, "privacy" ) )
   {
      if( !I3IS_SET( I3FLAG( ch ), I3_PRIVACY ) )
      {
         I3SET_BIT( I3FLAG( ch ), I3_PRIVACY );
         i3_to_char( "I3 finger privacy flag set.\r\n", ch );
         return;
      }
      I3REMOVE_BIT( I3FLAG( ch ), I3_PRIVACY );
      i3_to_char( "I3 finger privacy flag removed.\r\n", ch );
      return;
   }

   if( I3ISINVIS( ch ) )
   {
      i3_to_char( "You are invisible.\r\n", ch );
      return;
   }

   i3_strlcpy(atbuf, argument, SMST);
   if( ( ps = strchr( atbuf, '@' ) ) == NULL )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   ps[0] = '\0';
   i3_strlcpy( user, argument, SMST );
   i3_strlcpy( mud, ps + 1, SMST );

   if( user[0] == '\0' || mud[0] == '\0' )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( !( pmud = find_I3_mud_by_name( mud ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( !strcasecmp( I3_THISMUD, pmud->name ) )
   {
      i3_to_char( "Use your mud's own internal system for that.\r\n", ch );
      return;
   }

   if( pmud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", pmud->name );
      return;
   }

   if( pmud->finger == 0 )
      i3_printf( ch, "%s does not support the 'finger' command. Sending anyway.\r\n", pmud->name );

   I3_send_finger( ch, user, pmud->name );
}

I3_CMD( i3_emote )
{
   char to[SMST], *ps;
   char mud[SMST];
   I3_MUD *pmud;

   if( I3ISINVIS( ch ) )
   {
      i3_to_char( "You are invisible.\r\n", ch );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Usage: i3emoteto <person@mud> <emote message>\r\n", ch );
      return;
   }

   argument = i3_one_argument( argument, to );
   ps = strchr( to, '@' );

   if( to[0] == '\0' || argument[0] == '\0' || ps == NULL )
   {
      i3_to_char( "&YYou should specify a person and a mud.\r\n"
                  "(use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   ps[0] = '\0';
   ps++;
   i3_strlcpy( mud, ps, SMST );

   if( !( pmud = find_I3_mud_by_name( mud ) ) )
   {
      i3_to_char( "&YNo such mud known.\r\n" "( use &Wi3mudlist&Y to get an overview of the muds available)\r\n", ch );
      return;
   }

   if( pmud->status >= 0 )
   {
      i3_printf( ch, "%s is marked as down.\r\n", pmud->name );
      return;
   }

   if( pmud->emoteto == 0 )
      i3_printf( ch, "%s does not support the 'emoteto' command. Sending anyway.\r\n", pmud->name );

   i3_send_emote_to( ch, to, pmud, argument );
}

I3_CMD( i3_router )
{
   ROUTER_DATA *router;
   char cmd[SMST];

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3router add <router_name> <router_ip> <router_port>\r\n", ch );
      i3_to_char( "&wUsage: i3router remove <router_name>\r\n", ch );
      i3_to_char( "&wUsage: i3router list\r\n", ch );
      return;
   }
   argument = i3_one_argument( argument, cmd );

   if( !strcasecmp( cmd, "list" ) )
   {
      i3_to_char( "&RThe mud has the following routers configured:\r\n", ch );
      i3_to_char( "&WRouter Name     Router IP/DNS                  Router Port\r\n", ch );
      for( router = first_router; router; router = router->next )
         i3_printf( ch, "&c%-15.15s &c%-30.30s %d\r\n", router->name, router->ip, router->port );
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      *cmd = '\0';
      i3_router( ch, cmd );
      return;
   }

   if( !strcasecmp( cmd, "remove" ) )
   {
      for( router = first_router; router; router = router->next )
      {
         if( !strcasecmp( router->name, argument ) || !strcasecmp( router->ip, argument ) )
         {
            I3STRFREE( router->name );
            I3STRFREE( router->ip );
            I3UNLINK( router, first_router, last_router, next, prev );
            I3DISPOSE( router );
            i3_printf( ch, "&YRouter &W%s&Y has been removed from your configuration.\r\n", argument );
            I3_saverouters(  );
            return;
         }
      }
      i3_printf( ch, "&YNo router named &W%s&Y exists in your configuration.\r\n", argument );
      return;
   }

   if( !strcasecmp( cmd, "add" ) )
   {
      ROUTER_DATA *temp;
      char rtname[SMST];
      char rtip[SMST];
      int rtport;

      argument = i3_one_argument( argument, rtname );
      argument = i3_one_argument( argument, rtip );

      if( rtname[0] == '\0' || rtip[0] == '\0' || !argument || argument[0] == '\0' )
      {
         *rtname = '\0';
         i3_router( ch, rtname );
         return;
      }

      if( rtname[0] != '*' )
      {
         i3_to_char( "&YA router name must begin with a &W*&Y to be valid.\r\n", ch );
         return;
      }

      for( temp = first_router; temp; temp = temp->next )
      {
         if( !strcasecmp( temp->name, rtname ) )
         {
            i3_printf( ch, "&YA router named &W%s&Y is already in your configuration.\r\n", rtname );
            return;
         }
      }

      if( !is_number( argument ) )
      {
         i3_to_char( "&YPort must be a numerical value.\r\n", ch );
         return;
      }

      rtport = atoi( argument );
      if( rtport < 1 || rtport > 65535 )
      {
         i3_to_char( "&YInvalid port value specified.\r\n", ch );
         return;
      }

      I3CREATE( router, ROUTER_DATA, 1 );
      I3LINK( router, first_router, last_router, next, prev );
      router->name = I3STRALLOC( rtname );
      router->ip = I3STRALLOC( rtip );
      router->port = rtport;
      router->reconattempts = 0;
      I3LINK( router, first_router, last_router, next, prev );

      i3_printf( ch, "&YRouter: &W%s %s %d&Y has been added to your configuration.\r\n",
                 router->name, router->ip, router->port );
      I3_saverouters(  );
      return;
   }
   *cmd = '\0';
   i3_router( ch, cmd );
   return;
}

I3_CMD( i3_stats )
{
   I3_MUD *mud;
   I3_CHANNEL *channel;
   int mud_count = 0, chan_count = 0;

   for( mud = first_mud; mud; mud = mud->next )
      mud_count++;

   for( channel = first_I3chan; channel; channel = channel->next )
      chan_count++;

   i3_to_char( "&cGeneral Statistics:\r\n\r\n", ch );
   i3_printf( ch, "&cCurrently connected to: &W%s\r\n", i3_is_connected(  )? I3_ROUTER_NAME : "Nowhere!" );
   if( i3_is_connected(  ) )
      i3_printf( ch, "&cConnected on descriptor: &W%d\r\n", I3_socket );
   i3_printf( ch, "&cBytes sent    : &W%ld\r\n", bytes_sent );
   i3_printf( ch, "&cBytes received: &W%ld\r\n", bytes_received );
   i3_printf( ch, "&cKnown muds    : &W%d\r\n", mud_count );
   i3_printf( ch, "&cKnown channels: &W%d\r\n", chan_count );
   return;
}

I3_CMD( i3_help )
{
   I3_HELP_DATA *help;
   char buf[LGST];
   int col, perm;

   if( !argument || argument[0] == '\0' )
   {
      i3_strlcpy( buf, "&gHelp is available for the following commands:\r\n", LGST );
      i3_strlcat( buf, "&G---------------------------------------------\r\n", LGST );
      for( perm = I3PERM_MORT; perm <= I3PERM( ch ); perm++ )
      {
         col = 0;
         snprintf( buf + strlen( buf ), LGST - strlen( buf ), "\r\n&g%s helps:&G\r\n", perm_names[perm] );
         for( help = first_i3_help; help; help = help->next )
         {
            if( help->level != perm )
               continue;

            snprintf( buf + strlen( buf ), LGST - strlen( buf ), "%-15s", help->name );
            if( ++col % 6 == 0 )
               i3_strlcat( buf, "\r\n", LGST );
         }
         if( col % 6 != 0 )
            i3_strlcat( buf, "\r\n", LGST );
      }
      i3_send_to_pager( buf, ch );
      return;
   }

   for( help = first_i3_help; help; help = help->next )
   {
      if( !strcasecmp( help->name, argument ) )
      {
         if( !help->text || help->text[0] == '\0' )
            i3_printf( ch, "&gNo inforation available for topic &W%s&g.\r\n", help->name );
         else
            i3_printf( ch, "&g%s\r\n", help->text );
         return;
      }
   }
   i3_printf( ch, "&gNo help exists for topic &W%s&g.\r\n", argument );
   return;
}

I3_CMD( i3_hedit )
{
   I3_HELP_DATA *help;
   char name[SMST], cmd[SMST];
   bool found = FALSE;

   argument = i3_one_argument( argument, name );
   argument = i3_one_argument( argument, cmd );

   if( name[0] == '\0' || cmd[0] == '\0' || !argument || argument[0] == '\0' )
   {
      i3_to_char( "&wUsage: i3hedit <topic> [name|perm] <field>\r\n", ch );
      i3_to_char( "&wWhere <field> can be either name, or permission level.\r\n", ch );
      return;
   }

   for( help = first_i3_help; help; help = help->next )
   {
      if( !strcasecmp( help->name, name ) )
      {
         found = TRUE;
         break;
      }
   }

   if( !found )
   {
      i3_printf( ch, "&gNo help exists for topic &W%s&g. You will need to add it to the helpfile manually.\r\n", name );
      return;
   }

   if( !strcasecmp( cmd, "name" ) )
   {
      i3_printf( ch, "&W%s &ghas been renamed to &W%s.\r\n", help->name, argument );
      I3STRFREE( help->name );
      help->name = I3STRALLOC( argument );
      i3_savehelps(  );
      return;
   }

   if( !strcasecmp( cmd, "perm" ) )
   {
      int permvalue = get_permvalue( argument );

      if( !i3check_permissions( ch, permvalue, help->level, FALSE ) )
         return;

      i3_printf( ch, "&gPermission level for &W%s &ghas been changed to &W%s.\r\n", help->name, perm_names[permvalue] );
      help->level = permvalue;
      i3_savehelps(  );
      return;
   }
   *name = '\0';
   i3_hedit( ch, name );
   return;
}

I3_CMD( i3_other )
{
   I3_CMD_DATA *cmd;
   char buf[LGST];
   int col, perm;

   i3_strlcpy( buf, "&gThe following commands are available:\r\n", LGST );
   i3_strlcat( buf, "&G-------------------------------------\r\n", LGST );
   for( perm = I3PERM_MORT; perm <= I3PERM( ch ); perm++ )
   {
      col = 0;
      snprintf( buf + strlen( buf ), LGST - strlen( buf ), "\r\n&g%s commands:&G\r\n", perm_names[perm] );
      for( cmd = first_i3_command; cmd; cmd = cmd->next )
      {
         if( cmd->level != perm )
            continue;

         snprintf( buf + strlen( buf ), LGST - strlen( buf ), "%-15s", cmd->name );
         if( ++col % 6 == 0 )
            i3_strlcat( buf, "\r\n", LGST );
      }
      if( col % 6 != 0 )
         i3_strlcat( buf, "\r\n", LGST );
   }
   i3_send_to_pager( buf, ch );
   i3_send_to_pager( "\r\n&gFor information about a specific command, see &Wi3help <command>&g.\r\n", ch );
   return;
}

I3_CMD( i3_afk )
{
   if( I3IS_SET( I3FLAG( ch ), I3_AFK ) )
   {
      I3REMOVE_BIT( I3FLAG( ch ), I3_AFK );
      i3_to_char( "You are no longer AFK to I3.\r\n", ch );
   }
   else
   {
      I3SET_BIT( I3FLAG( ch ), I3_AFK );
      i3_to_char( "You are now AFK to I3.\r\n", ch );
   }
   return;
}

I3_CMD( i3_color )
{
   if( I3IS_SET( I3FLAG( ch ), I3_COLORFLAG ) )
   {
      I3REMOVE_BIT( I3FLAG( ch ), I3_COLORFLAG );
      i3_to_char( "I3 color is now off.\r\n", ch );
   }
   else
   {
      I3SET_BIT( I3FLAG( ch ), I3_COLORFLAG );
      i3_to_char( "&RI3 c&Yo&Gl&Bo&Pr &Ris now on. Enjoy :)\r\n", ch );
   }
   return;
}

I3_CMD( i3_cedit )
{
   I3_CMD_DATA *cmd, *tmp;
   I3_ALIAS *alias, *alias_next;
   char name[SMST], option[SMST];
   bool found = FALSE, aliasfound = FALSE;

   argument = i3_one_argument( argument, name );
   argument = i3_one_argument( argument, option );

   if( name[0] == '\0' || option[0] == '\0' )
   {
      i3_to_char( "Usage: i3cedit <command> <create|delete|alias|rename|code|permission|connected> <field>.\r\n", ch );
      return;
   }

   for( cmd = first_i3_command; cmd; cmd = cmd->next )
   {
      if( !strcasecmp( cmd->name, name ) )
      {
         found = TRUE;
         break;
      }
      for( alias = cmd->first_alias; alias; alias = alias->next )
      {
         if( !strcasecmp( alias->name, name ) )
            aliasfound = TRUE;
      }
   }

   if( !strcasecmp( option, "create" ) )
   {
      if( found )
      {
         i3_printf( ch, "&gA command named &W%s &galready exists.\r\n", name );
         return;
      }

      if( aliasfound )
      {
         i3_printf( ch, "&g%s already exists as an alias for another command.\r\n", name );
         return;
      }

      I3CREATE( cmd, I3_CMD_DATA, 1 );
      cmd->name = I3STRALLOC( name );
      cmd->level = I3PERM( ch );
      cmd->connected = FALSE;
      i3_printf( ch, "&gCommand &W%s &gcreated.\r\n", cmd->name );
      if( argument && argument[0] != '\0' )
      {
         cmd->function = i3_function( argument );
         if( cmd->function == NULL )
            i3_printf( ch, "&gFunction &W%s &gdoes not exist - set to NULL.\r\n", argument );
      }
      else
      {
         i3_to_char( "&gFunction set to NULL.\r\n", ch );
         cmd->function = NULL;
      }
      I3LINK( cmd, first_i3_command, last_i3_command, next, prev );
      I3_savecommands(  );
      return;
   }

   if( !found )
   {
      i3_printf( ch, "&gNo command named &W%s &gexists.\r\n", name );
      return;
   }

   if( !i3check_permissions( ch, cmd->level, cmd->level, FALSE ) )
      return;

   if( !strcasecmp( option, "delete" ) )
   {
      i3_printf( ch, "&gCommand &W%s &ghas been deleted.\r\n", cmd->name );
      for( alias = cmd->first_alias; alias; alias = alias_next )
      {
         alias_next = alias->next;

         I3UNLINK( alias, cmd->first_alias, cmd->last_alias, next, prev );
         I3STRFREE( alias->name );
         I3DISPOSE( alias );
      }
      I3UNLINK( cmd, first_i3_command, last_i3_command, next, prev );
      I3STRFREE( cmd->name );
      I3DISPOSE( cmd );
      I3_savecommands(  );
      return;
   }

   /*
    * MY GOD! What an inefficient mess you've made Samson! 
    */
   if( !strcasecmp( option, "alias" ) )
   {
      for( alias = cmd->first_alias; alias; alias = alias_next )
      {
         alias_next = alias->next;

         if( !strcasecmp( alias->name, argument ) )
         {
            i3_printf( ch, "&W%s &ghas been removed as an alias for &W%s\r\n", argument, cmd->name );
            I3UNLINK( alias, cmd->first_alias, cmd->last_alias, next, prev );
            I3STRFREE( alias->name );
            I3DISPOSE( alias );
            I3_savecommands(  );
            return;
         }
      }

      for( tmp = first_i3_command; tmp; tmp = tmp->next )
      {
         if( !strcasecmp( tmp->name, argument ) )
         {
            i3_printf( ch, "&W%s &gis already a command name.\r\n", argument );
            return;
         }
         for( alias = tmp->first_alias; alias; alias = alias->next )
         {
            if( !strcasecmp( argument, alias->name ) )
            {
               i3_printf( ch, "&W%s &gis already an alias for &W%s\r\n", argument, tmp->name );
               return;
            }
         }
      }

      I3CREATE( alias, I3_ALIAS, 1 );
      alias->name = I3STRALLOC( argument );
      I3LINK( alias, cmd->first_alias, cmd->last_alias, next, prev );
      i3_printf( ch, "&W%s &ghas been added as an alias for &W%s\r\n", alias->name, cmd->name );
      I3_savecommands(  );
      return;
   }

   if( !strcasecmp( option, "connected" ) )
   {
      cmd->connected = !cmd->connected;

      if( cmd->connected )
         i3_printf( ch, "&gCommand &W%s &gwill now require a connection to I3 to use.\r\n", cmd->name );
      else
         i3_printf( ch, "&gCommand &W%s &gwill no longer require a connection to I3 to use.\r\n", cmd->name );
      I3_savecommands(  );
      return;
   }

   if( !strcasecmp( option, "show" ) )
   {
      char buf[LGST];

      i3_printf( ch, "&gCommand       : &W%s\r\n", cmd->name );
      i3_printf( ch, "&gPermission    : &W%s\r\n", perm_names[cmd->level] );
      i3_printf( ch, "&gFunction      : &W%s\r\n", i3_funcname( cmd->function ) );
      i3_printf( ch, "&gConnection Req: &W%s\r\n", cmd->connected ? "Yes" : "No" );
      if( cmd->first_alias )
      {
         int col = 0;
         i3_strlcpy( buf, "&gAliases       : &W", LGST );
         for( alias = cmd->first_alias; alias; alias = alias->next )
         {
            snprintf( buf + strlen( buf ), LGST - strlen( buf ), "%s ", alias->name );
            if( ++col % 10 == 0 )
               i3_strlcat( buf, "\r\n", LGST );
         }
         if( col % 10 != 0 )
            i3_strlcat( buf, "\r\n", LGST );
         i3_to_char( buf, ch );
      }
      return;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_to_char( "Required argument missing.\r\n", ch );
      *name = '\0';
      i3_cedit( ch, name );
      return;
   }

   if( !strcasecmp( option, "rename" ) )
   {
      i3_printf( ch, "&gCommand &W%s &ghas been renamed to &W%s.\r\n", cmd->name, argument );
      I3STRFREE( cmd->name );
      cmd->name = I3STRALLOC( argument );
      I3_savecommands(  );
      return;
   }

   if( !strcasecmp( option, "code" ) )
   {
      cmd->function = i3_function( argument );
      if( cmd->function == NULL )
         i3_printf( ch, "&gFunction &W%s &gdoes not exist - set to NULL.\r\n", argument );
      else
         i3_printf( ch, "&gFunction set to &W%s.\r\n", argument );
      I3_savecommands(  );
      return;
   }

   if( !strcasecmp( option, "perm" ) || !strcasecmp( option, "permission" ) )
   {
      int permvalue = get_permvalue( argument );

      if( !i3check_permissions( ch, permvalue, cmd->level, FALSE ) )
         return;

      cmd->level = permvalue;
      i3_printf( ch, "&gCommand &W%s &gpermission level has been changed to &W%s.\r\n", cmd->name, perm_names[permvalue] );
      I3_savecommands(  );
      return;
   }
   *name = '\0';
   i3_cedit( ch, name );
   return;
}

char *i3_find_social( CHAR_DATA * ch, char *sname, char *person, char *mud, bool victim )
{
   static char socname[LGST];
#ifdef SMAUGSOCIAL
   SOCIAL_DATA *social;
   char *c;
#else
   int cmd;
   bool found;
#endif

   socname[0] = '\0';

#ifdef SMAUGSOCIAL
   for( c = sname; *c; *c = tolower( *c ), c++ );

   if( !( social = find_social( sname ) ) )
   {
      i3_printf( ch, "&YSocial &W%s&Y does not exist on this mud.\r\n", sname );
      return socname;
   }

   if( person && person[0] != '\0' && mud && mud[0] != '\0' )
   {
      if( person && person[0] != '\0' && !strcasecmp( person, CH_I3NAME( ch ) )
          && mud && mud[0] != '\0' && !strcasecmp( mud, I3_THISMUD ) )
      {
         if( !social->others_auto )
         {
            i3_printf( ch, "&YSocial &W%s&Y: Missing others_auto.\r\n", social->name );
            return socname;
         }
         i3_strlcpy( socname, social->others_auto, LGST );
      }
      else
      {
         if( !victim )
         {
            if( !social->others_found )
            {
               i3_printf( ch, "&YSocial &W%s&Y: Missing others_found.\r\n", social->name );
               return socname;
            }
            i3_strlcpy( socname, social->others_found, LGST );
         }
         else
         {
            if( !social->vict_found )
            {
               i3_printf( ch, "&YSocial &W%s&Y: Missing vict_found.\r\n", social->name );
               return socname;
            }
            i3_strlcpy( socname, social->vict_found, LGST );
         }
      }
   }
   else
   {
      if( !social->others_no_arg )
      {
         i3_printf( ch, "&YSocial &W%s&Y: Missing others_no_arg.\r\n", social->name );
         return socname;
      }
      i3_strlcpy( socname, social->others_no_arg, LGST );
   }
#else
   found = FALSE;
   for( cmd = 0; social_table[cmd].name[0] != '\0'; cmd++ )
   {
      if( sname[0] == social_table[cmd].name[0] && !i3_str_prefix( sname, social_table[cmd].name ) )
      {
         found = TRUE;
         break;
      }
   }

   if( !found )
   {
      i3_printf( ch, "&YSocial &W%s&Y does not exist on this mud.\r\n", sname );
      return socname;
   }

   if( person && person[0] != '\0' && mud && mud[0] != '\0' )
   {
      if( person && person[0] != '\0' && !strcasecmp( person, CH_I3NAME( ch ) )
          && mud && mud[0] != '\0' && !strcasecmp( mud, I3_THISMUD ) )
      {
         if( !social_table[cmd].others_auto )
         {
            i3_printf( ch, "&YSocial &W%s&Y: Missing others_auto.\r\n", sname );
            return socname;
         }
         i3_strlcpy( socname, social_table[cmd].others_auto, LGST );
      }
      else
      {
         if( !victim )
         {
            if( !social_table[cmd].others_found )
            {
               i3_printf( ch, "&YSocial &W%s&Y: Missing others_found.\r\n", sname );
               return socname;
            }
            i3_strlcpy( socname, social_table[cmd].others_found, LGST );
         }
         else
         {
            if( !social_table[cmd].vict_found )
            {
               i3_printf( ch, "&YSocial &W%s&Y: Missing vict_found.\r\n", sname );
               return socname;
            }
            i3_strlcpy( socname, social_table[cmd].vict_found, LGST );
         }
      }
   }
   else
   {
      if( !social_table[cmd].others_no_arg )
      {
         i3_printf( ch, "&YSocial &W%s&Y: Missing others_no_arg.\r\n", sname );
         return socname;
      }
      i3_strlcpy( socname, social_table[cmd].others_no_arg, LGST );
   }
#endif
   return socname;
}

/* Revised 10/10/03 by Xorith: Recognize the need to capitalize for a new sentence. */
char *i3act_string( const char *format, CHAR_DATA * ch, CHAR_DATA * vic )
{
   static const char *he_she[] = { "it", "he", "she" };
   static const char *him_her[] = { "it", "him", "her" };
   static const char *his_her[] = { "its", "his", "her" };
   static char buf[LGST];
   char tmp_str[LGST];
   const char *i = "";
   char *point;
   bool should_upper = FALSE;

   if( !format || format[0] == '\0' || !ch )
      return NULL;

   point = buf;

   while( *format != '\0' )
   {
      if( *format == '.' || *format == '?' || *format == '!' )
         should_upper = TRUE;
      else if( should_upper == TRUE && !isspace( *format ) && *format != '$' )
         should_upper = FALSE;

      if( *format != '$' )
      {
         *point++ = *format++;
         continue;
      }
      ++format;

      if( ( !vic ) && ( *format == 'N' || *format == 'E' || *format == 'M' || *format == 'S' || *format == 'K' ) )
         i = " !!!!! ";
      else
      {
         switch ( *format )
         {
            default:
               i = " !!!!! ";
               break;
            case 'n':
               i = "$N";
               break;
            case 'N':
               i = "$O";
               break;
            case 'e':
               i = should_upper ?
                  i3_capitalize( he_she[URANGE( 0, CH_I3SEX( ch ), 2 )] ) : he_she[URANGE( 0, CH_I3SEX( ch ), 2 )];
               break;

            case 'E':
               i = should_upper ?
                  i3_capitalize( he_she[URANGE( 0, CH_I3SEX( vic ), 2 )] ) : he_she[URANGE( 0, CH_I3SEX( vic ), 2 )];
               break;

            case 'm':
               i = should_upper ?
                  i3_capitalize( him_her[URANGE( 0, CH_I3SEX( ch ), 2 )] ) : him_her[URANGE( 0, CH_I3SEX( ch ), 2 )];
               break;

            case 'M':
               i = should_upper ?
                  i3_capitalize( him_her[URANGE( 0, CH_I3SEX( vic ), 2 )] ) : him_her[URANGE( 0, CH_I3SEX( vic ), 2 )];
               break;

            case 's':
               i = should_upper ?
                  i3_capitalize( his_her[URANGE( 0, CH_I3SEX( ch ), 2 )] ) : his_her[URANGE( 0, CH_I3SEX( ch ), 2 )];
               break;

            case 'S':
               i = should_upper ?
                  i3_capitalize( his_her[URANGE( 0, CH_I3SEX( vic ), 2 )] ) : his_her[URANGE( 0, CH_I3SEX( vic ), 2 )];
               break;

            case 'k':
               i3_one_argument( CH_I3NAME( ch ), tmp_str );
               i = ( char * )tmp_str;
               break;
            case 'K':
               i3_one_argument( CH_I3NAME( vic ), tmp_str );
               i = ( char * )tmp_str;
               break;
               break;
         }
      }
      ++format;
      while( ( *point = *i ) != '\0' )
         ++point, ++i;
   }
   *point = 0;
   point++;
   *point = '\0';

   buf[0] = UPPER( buf[0] );
   return buf;
}

CHAR_DATA *I3_make_skeleton( char *name )
{
   CHAR_DATA *skeleton;

   I3CREATE( skeleton, CHAR_DATA, 1 );

#ifdef I3CIRCLE
   skeleton->player.name = I3STRALLOC( name );
   skeleton->player.short_descr = I3STRALLOC( name );
   skeleton->in_room = real_room( 1 );
#else
   skeleton->name = I3STRALLOC( name );
   skeleton->short_descr = I3STRALLOC( name );
   skeleton->in_room = get_room_index( ROOM_VNUM_LIMBO );
#endif

   return skeleton;
}

void I3_purge_skeleton( CHAR_DATA * skeleton )
{
   if( !skeleton )
      return;

#ifdef I3CIRCLE
   I3STRFREE( skeleton->player.name );
   I3STRFREE( skeleton->player.short_descr );
#else
   I3STRFREE( skeleton->name );
   I3STRFREE( skeleton->short_descr );
#endif
   I3DISPOSE( skeleton );

   return;
}

void I3_send_social( I3_CHANNEL * channel, CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *skeleton = NULL;
   char *ps;
   char socbuf_o[LGST], socbuf_t[LGST], msg_o[LGST], msg_t[LGST];
   char arg1[SMST], person[SMST], mud[SMST], user[SMST], buf[LGST];
   unsigned int x;
   char atbuf[SMST];

   person[0] = '\0';
   mud[0] = '\0';

   /*
    * Name of social, remainder of argument is assumed to hold the target 
    */
   argument = i3_one_argument( argument, arg1 );

   snprintf( user, SMST, "%s@%s", CH_I3NAME( ch ), I3_THISMUD );
   if( !strcasecmp( user, argument ) )
   {
      i3_to_char( "Cannot target yourself due to the nature of I3 socials.\r\n", ch );
      return;
   }

   if( argument && argument[0] != '\0' )
   {
      i3_strlcpy(atbuf, argument, SMST);
      if( !( ps = strchr( atbuf, '@' ) ) )
      {
         i3_to_char( "You need to specify a person@mud for a target.\r\n", ch );
         return;
      }
      else
      {
         for( x = 0; x < strlen( argument ); x++ )
         {
            person[x] = argument[x];
            if( person[x] == '@' )
               break;
         }
         person[x] = '\0';

         ps[0] = '\0';
         i3_strlcpy( mud, ps + 1, SMST );
      }
   }

   snprintf( socbuf_o, LGST, "%s", i3_find_social( ch, arg1, person, mud, FALSE ) );

   if( socbuf_o[0] != '\0' )
      snprintf( socbuf_t, LGST, "%s", i3_find_social( ch, arg1, person, mud, TRUE ) );

   if( ( socbuf_o[0] != '\0' ) && ( socbuf_t[0] != '\0' ) )
   {
      if( argument && argument[0] != '\0' )
      {
         int sex;

         snprintf( buf, LGST, "%s@%s", person, mud );
         sex = I3_get_ucache_gender( buf );
         if( sex == -1 )
         {
            /*
             * Greg said to "just punt and call them all males".
             * * I decided to meet him halfway and at least request data before punting :)
             */
            I3_send_chan_user_req( mud, person );
            sex = SEX_MALE;
         }
         else
            sex = i3todikugender( sex );

         skeleton = I3_make_skeleton( buf );
         CH_I3SEX( skeleton ) = sex;
      }

      i3_strlcpy( msg_o, ( char * )i3act_string( socbuf_o, ch, skeleton ), LGST );
      i3_strlcpy( msg_t, ( char * )i3act_string( socbuf_t, ch, skeleton ), LGST );

      if( !skeleton )
         i3_send_channel_emote( channel, CH_I3NAME( ch ), msg_o );
      else
      {
         i3_strlcpy( buf, person, LGST );
         buf[0] = tolower( buf[0] );
         i3_send_channel_t( channel, CH_I3NAME( ch ), mud, buf, msg_o, msg_t, person );
      }
      if( skeleton )
         I3_purge_skeleton( skeleton );
   }
   return;
}

const char *i3_funcname( I3_FUN * func )
{
   if( func == i3_other )
      return ( "i3_other" );
   if( func == i3_listen_channel )
      return ( "i3_listen_channel" );
   if( func == i3_chanlist )
      return ( "i3_chanlist" );
   if( func == i3_mudlist )
      return ( "i3_mudlist" );
   if( func == i3_invis )
      return ( "i3_invis" );
   if( func == i3_who )
      return ( "i3_who" );
   if( func == i3_locate )
      return ( "i3_locate" );
   if( func == i3_tell )
      return ( "i3_tell" );
   if( func == i3_reply )
      return ( "i3_reply" );
   if( func == i3_emote )
      return ( "i3_emote" );
   if( func == i3_beep )
      return ( "i3_beep" );
   if( func == i3_ignorecmd )
      return ( "i3_ignorecmd" );
   if( func == i3_finger )
      return ( "i3_finger" );
   if( func == i3_mudinfo )
      return ( "i3_mudinfo" );
   if( func == i3_color )
      return ( "i3_color" );
   if( func == i3_afk )
      return ( "i3_afk" );
   if( func == i3_chan_who )
      return ( "i3_chan_who" );
   if( func == i3_connect )
      return ( "i3_connect" );
   if( func == i3_disconnect )
      return ( "i3_disconnect" );
   if( func == i3_reload )
      return ( "i3_reload" );
   if( func == i3_send_user_req )
      return ( "i3_send_user_req" );
   if( func == i3_permstats )
      return ( "i3_permstats" );
   if( func == i3_deny_channel )
      return ( "i3_deny_channel" );
   if( func == i3_permset )
      return ( "i3_permset" );
   if( func == i3_chanlayout )
      return ( "i3_chanlayout" );
   if( func == i3_admin_channel )
      return ( "i3_admin_channel" );
   if( func == i3_addchan )
      return ( "i3_addchan" );
   if( func == i3_removechan )
      return ( "i3_removechan" );
   if( func == i3_edit_channel )
      return ( "i3_edit_channel" );
   if( func == i3_mudlisten )
      return ( "i3_mudlisten" );
   if( func == i3_router )
      return ( "i3_router" );
   if( func == i3_bancmd )
      return ( "i3_bancmd" );
   if( func == i3_setconfig )
      return ( "i3_setconfig" );
   if( func == i3_setup_channel )
      return ( "i3_setup_channel" );
   if( func == i3_stats )
      return ( "i3_stats" );
   if( func == i3_show_ucache_contents )
      return ( "i3_show_ucache_contents" );
   if( func == i3_debug )
      return ( "i3_debug" );
   if( func == i3_hedit )
      return ( "i3_hedit" );
   if( func == i3_help )
      return ( "i3_help" );
   if( func == i3_cedit )
      return ( "i3_cedit" );

   return "";
}

I3_FUN *i3_function( const char *func )
{
   if( !strcasecmp( func, "i3_other" ) )
      return i3_other;
   if( !strcasecmp( func, "i3_listen_channel" ) )
      return i3_listen_channel;
   if( !strcasecmp( func, "i3_chanlist" ) )
      return i3_chanlist;
   if( !strcasecmp( func, "i3_mudlist" ) )
      return i3_mudlist;
   if( !strcasecmp( func, "i3_invis" ) )
      return i3_invis;
   if( !strcasecmp( func, "i3_who" ) )
      return i3_who;
   if( !strcasecmp( func, "i3_locate" ) )
      return i3_locate;
   if( !strcasecmp( func, "i3_tell" ) )
      return i3_tell;
   if( !strcasecmp( func, "i3_reply" ) )
      return i3_reply;
   if( !strcasecmp( func, "i3_emote" ) )
      return i3_emote;
   if( !strcasecmp( func, "i3_beep" ) )
      return i3_beep;
   if( !strcasecmp( func, "i3_ignorecmd" ) )
      return i3_ignorecmd;
   if( !strcasecmp( func, "i3_finger" ) )
      return i3_finger;
   if( !strcasecmp( func, "i3_mudinfo" ) )
      return i3_mudinfo;
   if( !strcasecmp( func, "i3_color" ) )
      return i3_color;
   if( !strcasecmp( func, "i3_afk" ) )
      return i3_afk;
   if( !strcasecmp( func, "i3_chan_who" ) )
      return i3_chan_who;
   if( !strcasecmp( func, "i3_connect" ) )
      return i3_connect;
   if( !strcasecmp( func, "i3_disconnect" ) )
      return i3_disconnect;
   if( !strcasecmp( func, "i3_reload" ) )
      return i3_reload;
   if( !strcasecmp( func, "i3_send_user_req" ) )
      return i3_send_user_req;
   if( !strcasecmp( func, "i3_permstats" ) )
      return i3_permstats;
   if( !strcasecmp( func, "i3_deny_channel" ) )
      return i3_deny_channel;
   if( !strcasecmp( func, "i3_permset" ) )
      return i3_permset;
   if( !strcasecmp( func, "i3_admin_channel" ) )
      return i3_admin_channel;
   if( !strcasecmp( func, "i3_bancmd" ) )
      return i3_bancmd;
   if( !strcasecmp( func, "i3_setconfig" ) )
      return i3_setconfig;
   if( !strcasecmp( func, "i3_setup_channel" ) )
      return i3_setup_channel;
   if( !strcasecmp( func, "i3_chanlayout" ) )
      return i3_chanlayout;
   if( !strcasecmp( func, "i3_addchan" ) )
      return i3_addchan;
   if( !strcasecmp( func, "i3_removechan" ) )
      return i3_removechan;
   if( !strcasecmp( func, "i3_edit_channel" ) )
      return i3_edit_channel;
   if( !strcasecmp( func, "i3_mudlisten" ) )
      return i3_mudlisten;
   if( !strcasecmp( func, "i3_router" ) )
      return i3_router;
   if( !strcasecmp( func, "i3_stats" ) )
      return i3_stats;
   if( !strcasecmp( func, "i3_show_ucache_contents" ) )
      return i3_show_ucache_contents;
   if( !strcasecmp( func, "i3_debug" ) )
      return i3_debug;
   if( !strcasecmp( func, "i3_help" ) )
      return i3_help;
   if( !strcasecmp( func, "i3_cedit" ) )
      return i3_cedit;
   if( !strcasecmp( func, "i3_hedit" ) )
      return i3_hedit;

   return NULL;
}

/*
 * This is how channels are interpreted. If they are not commands
 * or socials, this function will go through the list of channels
 * and send it to it if the name matches the local channel name.
 */
bool i3_command_hook( CHAR_DATA * ch, const char *lcommand, const char *argument )
{
   I3_CMD_DATA *cmd;
   I3_ALIAS *alias;
   I3_CHANNEL *channel;
   int x;

   if( IS_NPC( ch ) )
      return FALSE;

   if( !ch->desc )
      return FALSE;

   if( !this_i3mud )
   {
      i3_log( "%s", "Ooops. I3 called with missing configuration!" );
      return FALSE;
   }

   if( first_i3_command == NULL )
   {
      i3_log( "%s", "Dammit! No command data is loaded!" );
      return FALSE;
   }

   if( I3PERM( ch ) <= I3PERM_NONE )
   {
      i3_bug("Permission %d vs. %d", I3PERM(ch), I3PERM_NONE);
      return FALSE;
   }

#ifdef I3CIRCLE
   /*
    * CircleMUD parser leaves leading spaces after splitting one argument 
    */
   skip_spaces( &argument );
#endif

   /*
    * Simple command interpreter menu. Nothing overly fancy etc, but it beats trying to tie directly into the mud's
    * * own internal structures. Especially with the differences in codebases.
    */
   for( cmd = first_i3_command; cmd; cmd = cmd->next )
   {
      if( I3PERM( ch ) < cmd->level )
         continue;

      for( alias = cmd->first_alias; alias; alias = alias->next )
      {
         if( !strcasecmp( lcommand, alias->name ) )
         {
            lcommand = cmd->name;
            break;
         }
      }

      if( !strcasecmp( lcommand, cmd->name ) )
      {
         if( cmd->connected == TRUE && !i3_is_connected(  ) )
         {
            i3_to_char( "The mud is not currently connected to I3.\r\n", ch );
            return TRUE;
         }

         if( cmd->function == NULL )
         {
            i3_to_char( "That command has no code set. Inform the administration.\r\n", ch );
            i3_bug( "i3_command_hook: Command %s has no code set!", cmd->name );
            return TRUE;
         }

         ( *cmd->function ) ( ch, argument );
         return TRUE;
      }
   }

   /*
    * Assumed to be going for a channel if it gets this far 
    */

   if( !( channel = find_I3_channel_by_localname( lcommand ) ) )
      return FALSE;

   if( I3PERM( ch ) < channel->i3perm )
      return FALSE;

   if( i3_has_name( I3DENY( ch ), channel->local_name ) )
   {
      i3_printf( ch, "You have been denied the use of %s by the administration.\r\n", channel->local_name );
      return TRUE;
   }

   if( !argument || argument[0] == '\0' )
   {
      i3_printf( ch, "&cThe last %d %s messages:\r\n", MAX_I3HISTORY, channel->local_name );
      for( x = 0; x < MAX_I3HISTORY; x++ )
      {
         if( channel->history[x] != NULL )
            i3_printf( ch, "%s\r\n", channel->history[x] );
         else
            break;
      }
      return TRUE;
   }

   if( !i3_is_connected(  ) )
   {
      i3_to_char( "The mud is not currently connected to I3.\r\n", ch );
      return TRUE;
   }

   if( I3PERM( ch ) >= I3PERM_ADMIN && !strcasecmp( argument, "log" ) )
   {
      if( !I3IS_SET( channel->flags, I3CHAN_LOG ) )
      {
         I3SET_BIT( channel->flags, I3CHAN_LOG );
         i3_printf( ch, "&RFile logging enabled for %s, PLEASE don't forget to undo this when it isn't needed!\r\n",
                    channel->local_name );
      }
      else
      {
         I3REMOVE_BIT( channel->flags, I3CHAN_LOG );
         i3_printf( ch, "&GFile logging disabled for %s.\r\n", channel->local_name );
      }
      i3_write_channel_config(  );
      return TRUE;
   }

   if( !i3_has_name( I3LISTEN( ch ), channel->local_name ) )
   {
      i3_printf( ch, "&YYou were trying to send something to an I3 "
                 "channel but you're not listening to it.\r\nPlease use the command "
                 "'&Wi3listen %s&Y' to listen to it.\r\n", channel->local_name );
      return TRUE;
   }

   switch ( argument[0] )
   {
      case ',':
      case ':':
         /*
          * Strip the , and then extra spaces - Remcon 6-28-03 
          */
         argument++;
         while( isspace( *argument ) )
            argument++;
         i3_send_channel_emote( channel, CH_I3NAME( ch ), argument );
         break;
      case '@':
         /*
          * Strip the @ and then extra spaces - Remcon 6-28-03 
          */
         argument++;
         while( isspace( *argument ) )
            argument++;
         I3_send_social( channel, ch, argument );
         break;
      default:
         i3_send_channel_message( channel, CH_I3NAME( ch ), argument );
         break;
   }
   return TRUE;
}

char *i3_escape_name( const char *ps )
{
   static char xnew[LGST];
   char *pnew = xnew;
   char c;

   while( ps[0] )
   {
      c = (char) tolower((int)*ps);
      ps++;
      if(c < 'a' || c > 'z')
           continue;
      pnew[0] = c;
      pnew++;
   }
   pnew[0] = '\0';
   return xnew;
}

void i3_npc_chat( const char *chan_name, const char *actor, const char *message ) {
    I3_CHANNEL *channel;
    //char buf[LGST];

    if( !i3_is_connected(  ) ) {
        i3_log("Not connected!");
        return;
    }
    if( !( channel = find_I3_channel_by_localname( chan_name ) ) ) {
        i3_log("Can't find local channel %s.", chan_name);
        return;
    }

    while( isspace( *message ) )
        message++;
    i3_log("Sending [%s] from %s to %s.", message, actor, chan_name);
    i3_send_channel_emote( channel, actor, message );

#if 0
    if( strstr( message, "$N" ) == NULL )
        snprintf( buf, LGST, "$N %s", message );
    else
        i3_strlcpy( buf, message, LGST );

    i3_write_header( "channel-e", I3_THISMUD, actor, NULL, NULL );
    i3_write_buffer( "\"" );
    i3_write_buffer( channel->I3_name );
    i3_write_buffer( "\",\"" );
    i3_write_buffer( actor );
    i3_write_buffer( "\",\"" );
    send_to_i3( i3_escape( buf ) );
    i3_write_buffer( "\",})\r" );
    i3_send_packet(  );
    i3_log("Sending [%s] from %s to %s.", buf, actor, channel->I3_name);
#endif
}

