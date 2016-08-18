/* metar.c -- metar decoder
   $Id: main.c,v 1.9 2006/04/05 20:30:28 kees-guest Exp $
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
#include <curl/curl.h>
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "metar.h"

/* global variable so we dont have to mess with parameter passing */
char noaabuffer[METAR_MAXSIZE];

/* command line args */
int  decode=0;
int  verbose=0;

char *strupc(char *line) {
   char *p;
   for (p=line; *p; p++) *p=toupper(*p);
   return line;
}

/* show brief usage info */
void usage(char *name) {
	printf("$Id: main.c,v 1.9 2006/04/05 20:30:28 kees-guest Exp $\n");
	printf("Usage: %s options\n", name);
	printf("Options\n");
	printf("   -d        decode metar\n");
	printf("   -h        show this help\n");
	printf("   -v        be verbose\n");
	printf("Example: %s -d ehgr\n", name);
}


/* place NOAA data in buffer */
int receiveData(void *buffer, size_t size, size_t nmemb, void *stream) {
	size *= nmemb;
	size = (size <= METAR_MAXSIZE) ? size : METAR_MAXSIZE;
	strncpy(noaabuffer, buffer, size);
	return size;
}


/* fetch NOAA report */
int download_Metar(char *station) {
    CURL *curlhandle = NULL;
	CURLcode res;
    char url[URL_MAXSIZE];
	char tmp[URL_MAXSIZE];

    curlhandle = curl_easy_init();
	if (!curlhandle) return 1;

	memset(tmp, 0x0, URL_MAXSIZE);
	if (getenv("METARURL") == NULL) {
		strncpy(tmp, METARURL, URL_MAXSIZE);
	} else {
		strncpy(tmp, getenv("METARURL"), URL_MAXSIZE);
		if (verbose) printf("Using environment variable METARURL: %s\n", tmp);
	}

    if (snprintf(url, URL_MAXSIZE, "%s/%s.TXT", tmp, strupc(station)) < 0) 
        return 1;
	if (verbose) printf("Retrieving URL %s\n", url);

    curl_easy_setopt(curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, receiveData);
	memset(noaabuffer, 0x0, METAR_MAXSIZE);

	res = curl_easy_perform(curlhandle);
	curl_easy_cleanup(curlhandle);

    return 0;
}


/* decode metar */
void decode_Metar(metar_t metar) {
	cloudlist_t *curcloud;
	obslist_t   *curobs; 
	int n = 0;
	double qnh;

	printf("Station       : %s\n", metar.station);
	printf("Day           : %i\n", metar.day);
	printf("Time          : %02i:%02i UTC\n", metar.time/100, metar.time%100);
	if (metar.winddir == -1) {
		printf("Wind direction: Variable\n");
	} else {
		static const char *winddirs[] = {
			"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
			"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
	};
	n = ((metar.winddir * 4 + 45) / 90) % 16;
	printf("Wind direction: %i (%s)\n", metar.winddir, winddirs[n]);
	}
	printf("Wind speed    : %i %s\n", metar.windstr, metar.windunit);
	printf("Wind gust     : %i %s\n", metar.windgust, metar.windunit);
	printf("Visibility    : %i %s\n", metar.vis, metar.visunit);
	printf("Temperature   : %i C\n", metar.temp);
	printf("Dewpoint      : %i C\n", metar.dewp);

	qnh = metar.qnh;
	for (n = 0; n < metar.qnhfp; n++)
		qnh /= 10.0;
	printf("Pressure      : %.*f %s\n", metar.qnhfp, qnh, metar.qnhunit);
		
	printf("Clouds        : ");
	n = 0;
	for (curcloud = metar.clouds; curcloud != NULL; curcloud=curcloud->next) {
		if (n++ == 0) printf("%s at %d00 ft\n", 
				curcloud->cloud->type, curcloud->cloud->level);
		else printf("%15s %s at %d00 ft\n", 
				" ",curcloud->cloud->type, curcloud->cloud->level);
	}
	if (!n) printf("\n");

	printf("Phenomena     : ");
	n = 0;
	for (curobs = metar.obs; curobs != NULL; curobs=curobs->next) {
		if (n++ == 0) printf("%s\n", curobs->obs);
		else printf("%15s %s\n", " ",curobs->obs);
	}
	if (!n) printf("\n");
}


int main(int argc, char* argv[]) {
	int  res=0;
	metar_t metar;
	noaa_t  noaa;

	/* get options */
	opterr=0;
	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	while ((res = getopt(argc, argv, "hvd")) != -1) {
		switch (res) {
			case '?':
				usage(argv[0]);
				return 1;
				break;
			case 'h':
				usage(argv[0]);
				return 0;
				break;
			case 'd':
				decode=1;
				break;
			case 'v':
				verbose=1;
				break;
		}
	}
    
    curl_global_init(CURL_GLOBAL_DEFAULT);

	// clear out metar and noaa
	memset(&metar, 0x0, sizeof(metar_t));
	memset(&noaa, 0x0, sizeof(noaa_t));

	while (optind < argc) {
		res = download_Metar(argv[optind++]);
        if (res == 0) {
			parse_NOAA_data(noaabuffer, &noaa);
			printf("%s", noaa.report);
			if (decode) {
				parse_Metar(noaa.report, &metar);
				decode_Metar(metar);
			}
		} else 
			printf("Error: %d\n", res);
	}
    
    return 0;
}

// EOF
