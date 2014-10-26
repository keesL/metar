/* metar.c -- metar decoder
   $Id: metar.c,v 1.6 2006/10/30 22:18:58 kees-guest Exp $
   Copyright 2004,2005 Kees Leune <kees@leune.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metar.h"

extern int verbose;

struct observation {
	const char *code;
	const char *description;
};

typedef struct observation observation_t;

struct observation observations[] = {
	{"MI", "Shallow "},
	{"BL", "Blowing "},
	{"BC", "Patches "},
	{"SH", "Showers "},
	{"PR", "Partials "},
	{"DR", "Drifting "},
	{"TS", "Thunderstorm "},
	{"FZ", "Freezing "},
	{"DZ", "Drizzle "},
	{"IC", "Ice Crystals "},
	{"UP", "Unknown "},
	{"RA", "Rain "},
	{"PL", "Ice Pellets "},
	{"SN", "Snow "},
	{"GR", "Hail "},
	{"SG", "Snow Grains "},
	{"GS", "Small hail/snow pellets "},
	{"BR", "Mist "},
	{"SA", "Sand "},
	{"FU", "Smoke "},
	{"HZ", "Haze "},
	{"FG", "Fog "},
	{"VA", "Volcanic Ash "},
	{"PY", "Spray "},
	{"DU", "Widespread Dust "},
	{"SQ", "Squall "},
	{"FC", "Funnel Cloud "},
	{"SS", "Sand storm "},
	{"DS", "Dust storm "},
	{"PO", "Well developed dust/sand swirls "},
	{"VC", "Vicinity "}
};


/* Add a cloud to a list of clouds */
static void add_cloud(cloudlist_t **head, cloud_t *cloud) {
	cloudlist_t *current;

	if (*head == NULL) {
		*head = malloc(sizeof(cloudlist_t));
		current = *head;
		current->cloud = cloud;
		current->next = NULL;
		return;
	}
	current = *head;
	while (current->next != NULL) 
		current = current->next;
	
	current->next = (cloudlist_t *)malloc(sizeof(cloudlist_t));
	current = current->next;
	current->cloud = cloud;
	current->next = NULL;
} // add_cloud


/* Add observation */
static void add_observation(obslist_t **head, char *obs) {
	obslist_t *current;

	if (*head == NULL) {
		*head = malloc(sizeof(obslist_t));
		current = *head;
		current->obs = obs;
		current->next = NULL;
		return;
	}
	current = *head;
	while (current->next != NULL) 
		current = current->next;
	
	current->next = (obslist_t *)malloc(sizeof(obslist_t));
	current = current->next;
	current->obs = obs;
	current->next = NULL;
} // add_observation


/* build the observations regexp patters*/
static void get_observations_pattern(char *pattern, int len) {
	int i=0, p=0;
	int size = sizeof(observations) / sizeof(observation_t);

	for (i=0; i < size; i++) {
		// add observation to string if we have enough space left
		p = strlen(pattern);
		if (p + strlen(observations[i].code) < len)
			strcpy(&pattern[p], observations[i].code);

		// add separator
		p = strlen(pattern);
		if (p < len) pattern[p++] = '|';
	}

	// remove last |
	pattern[strlen(pattern)-1] = 0;
} 


/* get the description of code pattern */
static char *decode_obs(char *pattern) {
	int i=0;
	int size = sizeof(observations) / sizeof(observation_t);

	for (i=0; i < size; i++) 
		if (strncmp(pattern, observations[i].code, 2) == 0)
			return (char*) observations[i].description;

	return NULL;
}


/* Analyse the token which is provided and, when possible, set the
 * corresponding value in the metar struct
 */
static void analyse_token(char *token, metar_t *metar) {
	regex_t preg;
	regmatch_t pmatch[5];
	int size;
	char tmp[99];
	char obspattern[255];
	char obsp[275];

	if (verbose) printf("Parsing token `%s'\n", token);

	// find station
	if (metar->station[0] == 0) {
		if (regcomp(&preg, "^([A-Z]+)$", REG_EXTENDED)) {
			perror("parseMetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memcpy(metar->station, token+pmatch[1].rm_so, 
					(size < 10 ? size : 10));
			if (verbose) printf("   Found station %s\n", metar->station);
			return;
		}
	}

	// find day/time
	if ((int)metar->day == 0) {
		if (regcomp(&preg, "^([0-9]{2})([0-9]{4})Z$", REG_EXTENDED)) {
			perror("parseMetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", (int*)&metar->day);

			size = pmatch[2].rm_eo - pmatch[2].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", (int*)&metar->time);
			if (verbose) printf("   Found Day/Time %d/%d\n", 
					metar->day, metar->time);

			return;
		}
	} // daytime

	// find wind
	if ((int)metar->winddir == 0) {
		if (regcomp(&preg, "^(VRB|[0-9]{3})([0-9]{2})(G[0-9]+)?(KT)$", 
					REG_EXTENDED)) {
			perror("parseMetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memset(tmp, 0x0, 99);
			if (size) {
				memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
				if (strstr(tmp, "VRB") == NULL) { // winddir
					sscanf(tmp, "%d", (int*)&metar->winddir);
				} else { // vrb
					metar->winddir = -1;
				}
			}

			size = pmatch[2].rm_eo - pmatch[2].rm_so;
			memset(tmp, 0x0, 99);
			if (size) {
				memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
				sscanf(tmp, "%d", (int*)&metar->windstr);
			}

			size = pmatch[3].rm_eo - pmatch[3].rm_so;
			memset(tmp, 0x0, 99);
			if (size) {
				memcpy(tmp, token+pmatch[3].rm_so+1, (size < 99 ? size-1 : 99));
				sscanf(tmp, "%d", (int*)&metar->windgust);
			} else {
				metar->windgust = metar->windstr;
			}
			
			size = pmatch[4].rm_eo - pmatch[4].rm_so;
			if (size) {
				memcpy(&metar->windunit, token+pmatch[4].rm_so, 
					(size < 5 ? size : 5));
			}
			
			if (verbose) printf("   Found Winddir/str/gust/unit %d/%d/%d/%s\n", 
					metar->winddir, metar->windstr, metar->windgust,
					metar->windunit);
			return;
		}
	} // wind

	// find visibility
	if ((int)metar->vis == 0) {
		if (regcomp(&preg, "^([0-9]+)(SM)?$", REG_EXTENDED)) {
			perror("parsemetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", (int*)&metar->vis);
			
			size = pmatch[2].rm_eo - pmatch[2].rm_so;
			if (size) {
				memset(tmp, 0x0, 99);
				memcpy(&metar->visunit, token+pmatch[2].rm_so, 
						(size < 5 ? size : 5));
			} else 
				strncpy(metar->visunit, "M", 1);

			if (verbose) printf("   Visibility range/unit %d/%s\n", metar->vis, 
					metar->visunit);
			return;
		}
	} // visibility

	// find temperature and dewpoint
	if ((int)metar->temp == 0) {
		if (regcomp(&preg, "^(M?)([0-9]+)/(M?)([0-9]+)$", REG_EXTENDED)) {
			perror("parsemetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[2].rm_eo - pmatch[2].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", &metar->temp);

			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[1].rm_so, (size < 99 ? size : 99));
			if (strncmp(tmp, "M", 1) == 0) metar->temp = -metar->temp;
			
			size = pmatch[4].rm_eo - pmatch[4].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[4].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", &metar->dewp);
			
			size = pmatch[3].rm_eo - pmatch[3].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[3].rm_so, (size < 99 ? size : 99));
			if (strncmp(tmp, "M", 1) == 0) metar->dewp = -metar->dewp;

			if (verbose)
				printf("   Temp/dewpoint %d/%d\n", metar->temp, metar->dewp);
			return;
		}
	} // temp

	// find qnh
	if ((int)metar->qnh == 0) {
		if (regcomp(&preg, "^([QA])([0-9]+)$", REG_EXTENDED)) {
			perror("parsemetar");
			exit(errno);
		}
		if (!regexec(&preg, token, 5, pmatch, 0)) {
			size = pmatch[1].rm_eo - pmatch[1].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[1].rm_so, (size < 5 ? size : 5));
			if (strncmp(tmp, "Q", 1) == 0)
				strncpy(metar->qnhunit, "hPa", 3);
			else if (strncmp(tmp, "A", 1) == 0) {
				strncpy(metar->qnhunit, "\"Hg", 3);
				metar->qnhfp = 2;
			}
			else 
				strncpy(metar->qnhunit, "Unkn", 4);

			size = pmatch[2].rm_eo - pmatch[2].rm_so;
			memset(tmp, 0x0, 99);
			memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));
			sscanf(tmp, "%d", &metar->qnh);

			if (verbose) 
				printf("   Pressure/unit %d/%s\n", metar->qnh, metar->qnhunit);

			return;
		}
	} // qnh

	// multiple cloud layers possible
	if (regcomp(&preg, "^(SKC|FEW|SCT|BKN|OVC)([0-9]{3})$", REG_EXTENDED)) {
		perror("parsemetar");
		exit(errno);
	}
	if (!regexec(&preg, token, 5, pmatch, 0)) {
		cloud_t *cloud = malloc(sizeof(cloud_t));
		memset(cloud, 0x0, sizeof(cloud_t));

		size=pmatch[1].rm_eo - pmatch[1].rm_so;
		memcpy(&cloud->type, token+pmatch[1].rm_so, (size < 3 ? size : 3));
		
		size=pmatch[2].rm_eo - pmatch[2].rm_so;
		memset(tmp, 0x0, 99);
		memcpy(tmp, token+pmatch[2].rm_so, (size < 3 ? size : 3));
		sscanf(tmp, "%d", (int*)&cloud->level);

		add_cloud((cloudlist_t **)&metar->clouds, cloud);
		if (verbose)
			printf("   Cloud cover/alt %s/%d00\n", cloud->type, cloud->level);
		return;
	} // cloud
	
	// phenomena
	memset(obsp, 0x0, 275);
	memset(obspattern, 0x0, 255);
	get_observations_pattern(obspattern, 255);
	snprintf(obsp, 255, "^([+-]?)((%s)+)$", obspattern);

	// cannot to CAVOK as an observation in the array because it is more than
	// 2 characters long and that screw up my algorithm
	if (strstr(token, "CAVOK") != NULL) {
		add_observation((obslist_t **)&metar->obs, "Ceiling and visibility OK");
	};

	if (regcomp(&preg, obsp, REG_EXTENDED)) {
		perror("parsemetar");
		exit(errno);
	}
	if (!regexec(&preg, token, 5, pmatch, 0)) {
		char *obs;
		obs = malloc(99);
		memset(obs, 0x0, 99);

		size=pmatch[1].rm_eo - pmatch[1].rm_so;
		memset(tmp, 0x0, 99);
		memcpy(tmp, token+pmatch[1].rm_so, (size < 1 ? size : 1));
		if (tmp[0] == '-') strncpy(obs, "Light ", 99);
		else if (tmp[0] == '+') strncpy(obs, "Heavy ", 99);

		// split up in groups of 2 chars and decode per group
		size=pmatch[2].rm_eo - pmatch[2].rm_so;
		memset(tmp, 0x0, 99);
		memcpy(tmp, token+pmatch[2].rm_so, (size < 99 ? size : 99));

		int i=0;
		char code[2];
		while (i < strlen(tmp)) {
			memset(code, 0x0, 2);
			memcpy(code, tmp+i, 2);
			strncat(obs, decode_obs(code), 99);
			i += 2;
		}
		
		// remove trailing space
		obs[strlen(obs)-1]=0;
		add_observation((obslist_t **)&metar->obs, obs);
		if (verbose) 
			printf("   Phenomena %s\n", obs);
		
		return;
	}

	if (verbose) printf("   Unmatched token = %s\n", token);
}


/* PUBLIC--
 * Parse the METAR contain in the report string. Place the parsed report in
 * the metar struct.
 */
void parse_Metar(char *report, metar_t *metar) {
	char *token;
	char *last;

	// clear resutls
	memset(metar, 0x0, sizeof(metar_t));

	// strip trailing newlines
	while ((last = strrchr(report, '\n')) != NULL)
		memset(last, 0, 1);

	token = strtok(report, " ");
	while (token != NULL) {
		analyse_token(token, metar);
		token = strtok(NULL, " ");
	}

} // parse_Metar


/* parse the NOAA report contained in the noaa_data buffer. Place a parsed
 * data in the metar struct. 
 */
void parse_NOAA_data(char *noaa_data, noaa_t *noaa) {
	regex_t preg;
	regmatch_t pmatch[10];
	int size;

	if (regcomp(&preg, "^([0-9/]+ [0-9:]+)[[:space:]]+(.*)$", REG_EXTENDED)) {
		fprintf(stderr, "Unable to compile regular expression.\n");
		exit(100);
	}

	if (regexec(&preg, noaa_data, 10, pmatch, 0)) {
		fprintf(stderr, "METAR pattern not found in NOAA data.\n");
	} else {
		memset(noaa, 0x0, sizeof(noaa_t));
		/* date */
        size = pmatch[1].rm_eo - pmatch[1].rm_so;
        memcpy(noaa->date, noaa_data+pmatch[1].rm_so, (size < 36 ? size : 36));

		/* metar */
		size = pmatch[2].rm_eo - pmatch[2].rm_so;
		memcpy(noaa->report, noaa_data+pmatch[2].rm_so, 
				(size < 1024 ? size : 1024));
	}
} // parse_NOAA_data
