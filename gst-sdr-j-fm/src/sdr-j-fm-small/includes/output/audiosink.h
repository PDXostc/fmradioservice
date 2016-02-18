#
/*
 *    Copyright (C)  2009, 2010, 2011
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License. 
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __AUDIO_SINK
#define	__AUDIO_SINK
#include	"fm-constants.h"
#include	"ringbuffer.h"
#include	<pthread.h>

#define		LOWLATENCY	0100
#define		HIGHLATENCY	0200
#define		VERYHIGHLATENCY	0300

class	audioSink  {
public:
			audioSink		();
			~audioSink		(void);
	int32_t		putSample		(DSPCOMPLEX);
	int32_t		putSamples		(DSPCOMPLEX *, int32_t);
	int32_t	        getSamples              (DSPFLOAT *, uint32_t);
	int32_t		capacity		(void);
	int32_t		waiting			(void);
	void		cancelGet		(void);
	void		flush			(void);
private:
	int		wait			(int32_t = 30);
	void		signal			(bool cancel = false);
	int32_t		size;
	uint8_t		Latency;
	int16_t		bufSize;
	RingBuffer<float>	*_O_Buffer;
	pthread_mutex_t	lock;
	pthread_cond_t	sig;
	bool		cancelled;
};

#endif

