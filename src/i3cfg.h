/* Codebase macros - Change as you might need.
 * Yes, Rogel, you can gloat all you want. You win, this is cleaner, though not by a whole lot.
 */

#ifndef __I3CFG_H__
#define __I3CFG_H__

#ifndef I3CIRCLE
   #define CH_I3DATA(ch)           ((ch)->pcdata->i3chardata)
   #define CH_I3LEVEL(ch)          ((ch)->level)
   #define CH_I3NAME(ch)           ((ch)->name)
   #define CH_I3SEX(ch)            ((ch)->sex)
   #define CH_I3TITLE(ch)          ((ch)->pcdata->title)
#endif

#if defined(I3SMAUG) || defined(I3CHRONICLES)
   #define SMAUGSOCIAL
   #define SOCIAL_DATA SOCIALTYPE
   #define CH_I3RANK(ch)           ((ch)->pcdata->rank)
#endif

#ifdef I3ROM
   #define first_descriptor descriptor_list
   #define CH_I3RANK(ch)           (title_table[(ch)->class][(ch)->level][(ch)->sex == SEX_FEMALE ? 1 : 0])
#endif

#ifdef I3MERC
   #define first_descriptor descriptor_list
   #define CH_I3RANK(ch)           (title_table[(ch)->class][(ch)->level][(ch)->sex == SEX_FEMALE ? 1 : 0])
#endif

#ifdef I3UENVY
   #define SMAUGSOCIAL
   #define SOCIAL_DATA SOC_INDEX_DATA
   SOC_INDEX_DATA *find_social( char *command );
   #define first_descriptor descriptor_list
   #define I3MAXPLAYERS sysdata.max_players
   #define CH_I3DATA(ch)      ((ch)->pcdata->i3chardata)
   #define CH_I3LEVEL(ch)     ((ch)->level)
   #define CH_I3NAME(ch)      ((ch)->name)
   #define CH_I3TITLE(ch)     ((ch)->pcdata->title)
   #define CH_I3RANK(ch)      (title_table[(ch)->class][(ch)->level][(ch)->sex == SEX_FEMALE ? 1 : 0])
   #define CH_I3SEX(ch)       ((ch)->sex)
#endif

#ifdef I3UENVY
   #define SMAUGSOCIAL
   #define SOCIAL_DATA SOC_INDEX_DATA
   SOC_INDEX_DATA *find_social( char *command );
   #define first_descriptor descriptor_list
   #define CH_I3RANK(ch)           (title_table[(ch)->class][(ch)->level][(ch)->sex == SEX_FEMALE ? 1 : 0])
#endif

#ifdef I3ACK
   #define first_descriptor first_desc
   #define CH_I3RANK(ch)           (class_table[(ch)->class].who_name)
#endif

/* Blasted circle, always gotta do thing the hard way! */
#ifdef I3CIRCLE
   #define SMAUGSOCIAL

   /* This should be in an act.social.h, if it existed. Introducing
      it in an I3 patch would be too intrusive. */
   struct social_messg 
   {
      int act_nr;
      int hide;
      int min_victim_position;
      char *char_no_arg;
      char *others_no_arg;
      char *char_found;
      char *others_found;
      char *vict_found;
      char *not_found;
      char *char_auto;
      char *others_auto;
   };

   extern struct social_messg *soc_mess_list;
   struct social_messg *find_social( const char *name );

   typedef struct social_messg SOCIAL_DATA;
   typedef struct char_data CHAR_DATA;
   typedef struct descriptor_data DESCRIPTOR_DATA;

   extern const char *class_abbrevs[];

   const char *title_female( int chclass, int level );
   const char *title_male( int chclass, int level );

   #define first_descriptor         descriptor_list
   #define URANGE(a, b, c)          ((b) < (a) ? (a) : ((b) > (c) ? (c) : (b)))
   #define CH_I3DATA(ch)           ((ch)->player_specials->i3chardata)
   #define CH_I3LEVEL(ch)          GET_LEVEL(ch)
   #define CH_I3NAME(ch)           GET_NAME(ch)
   #define CH_I3TITLE(ch)          GET_TITLE(ch)
   #define CH_I3RANK(ch)           (GET_SEX(ch) == SEX_FEMALE ? title_female(GET_CLASS(ch), GET_LEVEL(ch))	\
								: title_male(GET_CLASS(ch), GET_LEVEL(ch)))
   #define CH_I3SEX(ch)            GET_SEX(ch)
#endif

#endif
