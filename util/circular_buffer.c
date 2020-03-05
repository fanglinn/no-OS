/***************************************************************************//**
 *   @file   circular_buffer.h
 *   @brief  Circular buffer implementation
 *   @author Mihail Chindris (mihail.chindris@analog.com)
********************************************************************************
 *   @copyright
 * Copyright 2020(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/

#include "circular_buffer.h"
#include "error.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/

/**
 * @struct circular_buffer
 * @brief Circular buffer descriptor
 */
struct circular_buffer {
	/** Size of the buffer in bytes */
	uint32_t	size;
	/** Number of user elements in the buffer */
	uint32_t	nb_elem;
	/** Size of an user element */
	uint32_t	elem_size;
	/** Address to the buffer */
	int8_t		*buff;
	/** Pointer to reading location */
	int8_t		*read_ptr;
	/** Pointer to writing location */
	int8_t		*write_ptr;
	/** Set when the buffer is full */
	bool		full;
};

/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/

int32_t cb_init(struct circular_buffer **desc, uint32_t nb_elements,
		uint32_t element_size)
{
	struct circular_buffer	*ldesc;
	uint32_t		size;

	if (!desc || !element_size || !nb_elements)
		return FAILURE;

	size = nb_elements * element_size;
	if (size < nb_elements || size < element_size)
		return FAILURE; //Integer overflow

	ldesc = (struct circular_buffer*)calloc(1, sizeof(*ldesc));
	if (!ldesc)
		return FAILURE;

	ldesc->buff = calloc(nb_elements, element_size);
	if (!ldesc->buff) {
		free(ldesc);
		return FAILURE;
	}

	ldesc->size = size;
	ldesc->elem_size = element_size;
	ldesc->nb_elem = 0;
	ldesc->read_ptr = ldesc->buff;
	ldesc->write_ptr = ldesc->buff;

	*desc = ldesc;

	return SUCCESS;
}

int32_t cb_remove(struct circular_buffer *desc)
{
	if (!desc)
		return FAILURE;

	if (desc->buff)
		free(desc->buff);
	free(desc);

	return SUCCESS;
}

/* Convert pointer in buffer to offset from the beginning */
static inline uint32_t ptr_to_offset(struct circular_buffer *desc, int8_t *ptr)
{
	return ptr - desc->buff;
}

/* Get size of the datt written to the buffer */
static inline uint32_t get_unread_size(struct circular_buffer *desc)
{
	uint32_t		size;

	if (desc->full)
		return desc->size;

	if (desc->write_ptr < desc->read_ptr) {
		size = desc->size - ptr_to_offset(desc, desc->read_ptr);
		size += desc->write_ptr - desc->buff;
	} else {
		size = desc->write_ptr - desc->read_ptr;
	}

	return size;
}

int32_t cb_size(struct circular_buffer *desc, uint32_t *nb_elements)
{
	if (!desc)
		return FAILURE;

	*nb_elements =  desc->nb_elem;

	return SUCCESS;
}

int32_t cb_write(struct circular_buffer *desc, const void *data,
		 uint32_t nb_elements)
{
	uint32_t		to_write_size;
	uint32_t		size_to_end;

	if (!desc || !nb_elements)
		return FAILURE;

	if (desc->full)
		return FAILURE;

	to_write_size = nb_elements * desc->elem_size;
	/* Check for overflow */
	if (to_write_size + (desc->nb_elem * desc->elem_size) > desc->size)
		return FAILURE;
	desc->nb_elem += nb_elements;

	size_to_end = desc->size - ptr_to_offset(desc, desc->write_ptr);
	if (to_write_size > size_to_end) {
		memcpy(desc->write_ptr, data, size_to_end);
		to_write_size -= size_to_end;
		memcpy(desc->buff, (uint8_t *)data + size_to_end,
		       to_write_size);
		desc->write_ptr = desc->buff + to_write_size;
	} else {
		memcpy(desc->write_ptr, data, to_write_size);
		desc->write_ptr += to_write_size;
	}

	if (desc->write_ptr == desc->read_ptr)
		desc->full = true;

	return SUCCESS;
}


int32_t cb_read(struct circular_buffer *desc, void *data,
		uint32_t nb_elements)
{
	uint32_t		to_read_size;
	uint32_t		size_to_end;

	if (!desc || !nb_elements)
		return FAILURE;

	/* Check for overflow */
	if (nb_elements > desc->nb_elem)
		return FAILURE;
	desc->nb_elem -= nb_elements;

	to_read_size = nb_elements * desc->elem_size;
	size_to_end = desc->size - ptr_to_offset(desc, desc->read_ptr);
	if (to_read_size > size_to_end) {
		memcpy(data, desc->read_ptr, size_to_end);
		to_read_size -= size_to_end;
		memcpy((int8_t *)data + size_to_end, desc->buff, to_read_size);
		desc->read_ptr = desc->buff + to_read_size;
	} else {
		memcpy(data, desc->read_ptr, to_read_size);
		desc->read_ptr += to_read_size;
	}

	if (desc->full)
		desc->full = false;

	return SUCCESS;
}

