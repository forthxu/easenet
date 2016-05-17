//=====================================================================
//
// inetsim.c - network simulator for IP-layer
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "inetsim.h"


/*====================================================================*/
/* INTERFACE DEFINITION                                               */
/*====================================================================*/

//---------------------------------------------------------------------
// ������·����ʼ��
//---------------------------------------------------------------------
void isim_transfer_init(iSimTransfer *trans, long rtt, long lost, long amb, 
		long limit, int mode)
{
	assert(trans);
	trans->rtt = rtt;
	trans->lost = lost;
	trans->amb = amb;
	trans->limit = limit;
	trans->seed = 0;
	trans->size = 0;
	trans->current = 0;
	trans->cnt_send = 0;
	trans->cnt_drop = 0;
	trans->mode = mode;
	iqueue_init(&trans->head);
}

//---------------------------------------------------------------------
// ������·������
//---------------------------------------------------------------------
void isim_transfer_destroy(iSimTransfer *trans)
{
	assert(trans);
	while (!iqueue_is_empty(&trans->head)) {
		struct IQUEUEHEAD *head = trans->head.next;
		iSimPacket *packet = iqueue_entry(head, iSimPacket, head);
		iqueue_del(head);
		free(packet);
	}
	trans->size = 0;
	trans->cnt_send = 0;
	trans->cnt_drop = 0;
	iqueue_init(&trans->head);
}

//---------------------------------------------------------------------
// ������·������ʱ��
//---------------------------------------------------------------------
void isim_transfer_settime(iSimTransfer *trans, unsigned long time)
{
	assert(trans);
	trans->current = time;
}

//---------------------------------------------------------------------
// ������·�������
//---------------------------------------------------------------------
long isim_transfer_random(iSimTransfer *trans, long range)
{
	unsigned long seed, value;

	assert(trans);

	if (range <= 0) return 0;

	seed = trans->seed;
	value = (((seed = seed * 214013L + 2531011L) >> 16) & 0xffff);
	trans->seed = seed;

	return value % range;
}

//---------------------------------------------------------------------
// ������·����������
//---------------------------------------------------------------------
long isim_transfer_send(iSimTransfer *trans, const void *data, long size)
{
	iSimPacket *packet;
	iqueue_head *p;
	long feature;
	long wave;

	trans->cnt_send++;

	// �ж��Ƿ񳬹�����
	if (trans->size >= trans->limit) {
		trans->cnt_drop++;
		return -1;
	}

	// �ж��Ƿ񶪰�
	if (trans->lost > 0) {
		if (isim_transfer_random(trans, 100) < trans->lost) {
			trans->cnt_drop++;
			return -2;
		}
	}

	// ���������ݰ�
	packet = (iSimPacket*)malloc(sizeof(iSimPacket) + size);
	assert(packet);

	packet->data = ((unsigned char*)packet) + sizeof(iSimPacket);
	packet->size = size;

	memcpy(packet->data, data, size);

	// ���㵽��ʱ��
	wave = (trans->rtt * trans->amb) / 100;
	wave = (wave * (isim_transfer_random(trans, 200) - 100)) / 100;
	wave = wave + trans->rtt;

	if (wave < 0) feature = trans->current;
	else feature = trans->current + wave;

	packet->timestamp = feature;

	// ������ʱ���Ⱥ����ʱ������
	for (p = trans->head.prev; p != &trans->head; p = p->prev) {
		iSimPacket *node = iqueue_entry(p, iSimPacket, head);
		if (node->timestamp < packet->timestamp) break;
	}
	
	// �����˳��ģʽ
	if (trans->mode != 0) p = trans->head.prev;

	iqueue_add(&packet->head, p);
	trans->size++;

	return 0;
}

//---------------------------------------------------------------------
// ������·����������
//---------------------------------------------------------------------
long isim_transfer_recv(iSimTransfer *trans, void *data, long maxsize)
{
	iSimPacket *packet;
	iqueue_head *p;
	long size = 0;

	assert(trans);

	// û�����ݰ�
	if (iqueue_is_empty(&trans->head)) {
		return -1;
	}

	p = trans->head.next;
	packet = iqueue_entry(p, iSimPacket, head);

	// ��Ϊ�������ʱ��
	if (trans->current < packet->timestamp) {
		return -2;
	}

	// �Ƴ�����
	iqueue_del(p);
	trans->size--;

	// ���ݿ���
	if (data) {
		size = packet->size;
		if (size > maxsize) size = maxsize;
		memcpy(data, packet->data, size);
	}

	// �ͷ��ڴ�
	free(packet);

	return size;
}


//---------------------------------------------------------------------
// isim_init:
// ��ʼ������ģ����
// rtt   - ����ʱ��ƽ����
// lost  - �����ʰٷֱ� (0 - 100)
// amb   - ʱ������ٷֱ� (0 - 100)
// limit - ������������
// mode  - 0(�󷢰����ȵ�) 1(�󷢰���Ȼ�󵽴�)
// ����ʱ��  = ��ǰʱ�� + rtt * 0.5 + rtt * (amb * 0.01) * random(-0.5, 0.5)
// ��������  = rtt( 60), lost( 5), amb(30), limit(1000)
// ��������  = rtt(120), lost(10), amb(40), limit(1000)
// ������ͨ  = rtt(200), lost(10), amb(50), limit(1000)
// ��������  = rtt(800), lost(20), amb(60), limit(1000)
//---------------------------------------------------------------------
void isim_init(iSimNet *simnet, long rtt, long lost, long amb, long limit, int mode)
{
	assert(simnet);
	isim_transfer_init(&simnet->t1, rtt >> 1, lost, amb, limit, mode);
	isim_transfer_init(&simnet->t2, rtt >> 1, lost, amb, limit, mode);
	simnet->p1.t1 = &simnet->t1;
	simnet->p1.t2 = &simnet->t2;
	simnet->p2.t1 = &simnet->t2;
	simnet->p2.t2 = &simnet->t1;
}

//---------------------------------------------------------------------
// ɾ������ģ����
//---------------------------------------------------------------------
void isim_destroy(iSimNet *simnet)
{
	assert(simnet);
	assert(simnet->p1.t1 && simnet->p1.t2);
	assert(simnet->p2.t1 && simnet->p2.t2);

	isim_transfer_destroy(&simnet->t1);
	isim_transfer_destroy(&simnet->t2);

	simnet->p1.t1 = NULL;
	simnet->p1.t2 = NULL;
	simnet->p2.t1 = NULL;
	simnet->p2.t2 = NULL;
}

//---------------------------------------------------------------------
// ����ʱ��
//---------------------------------------------------------------------
void isim_settime(iSimNet *simnet, unsigned long current)
{
	assert(simnet);
	isim_transfer_settime(&simnet->t1, current);
	isim_transfer_settime(&simnet->t2, current);
}

//---------------------------------------------------------------------
// ��������
//---------------------------------------------------------------------
long isim_send(iSimPeer *peer, const void *data, long size)
{
	return isim_transfer_send(peer->t1, data, size);
}

//---------------------------------------------------------------------
// ��������
//---------------------------------------------------------------------
long isim_recv(iSimPeer *peer, void *data, long maxsize)
{
	return isim_transfer_recv(peer->t2, data, maxsize);
}

//---------------------------------------------------------------------
// ȡ�ö˵㣺peerno = 0(�˵�1), 1(�˵�2)
//---------------------------------------------------------------------
iSimPeer *isim_peer(iSimNet *simnet, int peerno)
{
	assert(simnet);
	assert(peerno == 0 || peerno == 1);
	if (peerno == 0) return &simnet->p1;
	return &simnet->p2;
}

//---------------------------------------------------------------------
// �������������
//---------------------------------------------------------------------
void isim_seed(iSimNet *simnet, unsigned long seed1, unsigned long seed2)
{
	assert(simnet);
	simnet->t1.seed = seed1;
	simnet->t2.seed = seed2;
}

