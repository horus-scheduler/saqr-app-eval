/*
 * Copyright 2018-19 Board of Trustees of Stanford University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * networker.c - networking core functionality
 *
 * A single core is responsible for receiving all network packets in the
 * system and forwading them to the dispatcher.
 */
#include <stdio.h>

#include <ix/vm.h>
#include <ix/cfg.h>
#include <ix/log.h>
#include <ix/mbuf.h>
#include <ix/dispatch.h>
#include <ix/ethqueue.h>
#include <ix/transmit.h>

#include <asm/chksum.h>

#include <net/ip.h>
#include <net/udp.h>
#include <net/ethernet.h>

/**
 * do_networking - implements networking core's functionality
 * @parham: Receives packets from eth, and  fills the networker_pointer array, also removes the ones that are already done.
 * 
 */
void do_networking(void)
{
	int i, j, num_recv;
	uint8_t core_id;
	
	rqueue.head = NULL;
	while (1)
	{
		eth_process_poll();
		num_recv = eth_process_recv();
		if (num_recv == 0)
			continue;
		while (networker_pointers.cnt != 0)
			;
		for (i = 0; i < networker_pointers.free_cnt; i++)
		{
			struct request *req = networker_pointers.reqs[i];
			for (j = 0; j < req->pkts_length; j++)
			{
				mbuf_free(req->mbufs[j]);
			}
			mempool_free(&request_mempool, req);
		}
		networker_pointers.free_cnt = 0;
		j = 0;

		for (i = 0; i < num_recv; i++)
		{
			// @SAQR: pass core_id by ref so when parsing Saqr headers in rq_update, it will fill it based on dst_id
			struct request *req = rq_update(&rqueue, recv_mbufs[i], &core_id);

			if (req)
			{
				networker_pointers.reqs[j] = req;
				networker_pointers.types[j] = core_id; // core_id makes task to be queued in its dedicated queue (each worker has its queue)
				//log_info("core_id: %u\n", (unsigned int) core_id);
				j++;
			}
		}
		networker_pointers.cnt = j;
	}
}
