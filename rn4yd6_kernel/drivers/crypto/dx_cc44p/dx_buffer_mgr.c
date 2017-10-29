/***************************************************************
*  Copyright 2014 (c) Discretix Technologies Ltd.              *
*  This software is protected by copyright, international      *
*  treaties and various patents. Any copy, reproduction or     *
*  otherwise use of this software must be authorized in a      *
*  license agreement and include this Copyright Notice and any *
*  other notices specified in the license agreement.           *
*                                                              *
*  This software shall be governed by, and may be used and     *
*  redistributed under the terms and conditions of the GNU     *
*  General Public License version 2, as published by the       *
*  Free Software Foundation.                                   *
*                                                              *
*  This software is distributed in the hope that it will be    *
*  useful, but WITHOUT ANY liability and WARRANTY; without     *
*  even the implied warranty of MERCHANTABILITY or FITNESS     *
*  FOR A PARTICULAR PURPOSE. See the GNU General Public        *
*  License for more details.                                   *
*                                                              *
*  You should have received a copy of the GNU General          *
*  Public License along with this software; if not, please     *
*  write to the Free Software Foundation, Inc.,                *
*  59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   *
****************************************************************/

#include <linux/crypto.h>
#include <linux/version.h>
#include <crypto/algapi.h>
#include <crypto/aead.h>
#include <crypto/hash.h>
#include <crypto/authenc.h>
#include <crypto/scatterwalk.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/crypto.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "dx_buffer_mgr.h"
#include "sep_lli.h"
#include "dx_cipher.h"
#include "dx_hash.h"
#include "dx_aead.h"

#define LLI_MAX_NUM_OF_DATA_ENTRIES 128
#define LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES 4
#define MLLI_TABLE_MIN_ALIGNMENT 4 /*Force the MLLI table to be align to uint32 */
#define MAX_NUM_OF_BUFFERS_IN_MLLI 4

#ifdef DX_DEBUG
#define DUMP_SGL(sgl) \
	while (sg) { \
		DX_LOG_DEBUG("page=%lu offset=%u length=%u (dma_len=%u) " \
			     "dma_addr=%08x\n", (sg)->page_link, (sg)->offset, \
			(sg)->length, sg_dma_len(sg), (sg)->dma_address); \
		(sg) = scatterwalk_sg_next(sg); \
	}
#define DUMP_MLLI_TABLE(mlli_p, nents) \
	do { \
		DX_LOG_DEBUG("mlli=%pK nents=%u\n", (mlli_p), (nents)); \
		while((nents)--) { \
			DX_LOG_DEBUG("addr=0x%08X size=0x%08X\n", \
			     (mlli_p)[SEP_LLI_ADDR_WORD_OFFSET], \
			     (mlli_p)[SEP_LLI_SIZE_WORD_OFFSET]); \
			(mlli_p) += SEP_LLI_ENTRY_WORD_SIZE; \
		} \
	} while (0)
#define GET_DMA_BUFFER_TYPE(buff_type) ( \
	((buff_type) == DX_DMA_BUF_NULL) ? "BUF_NULL" : \
	((buff_type) == DX_DMA_BUF_DLLI) ? "BUF_DLLI" : \
	((buff_type) == DX_DMA_BUF_MLLI) ? "BUF_MLLI" : "BUF_INVALID")
#else
#define DX_BUFFER_MGR_DUMP_SGL(sgl)
#define DX_BUFFER_MGR_DUMP_MLLI_TABLE(mlli_p, nents)
#define GET_DMA_BUFFER_TYPE(buff_type)
#endif


enum dma_buffer_type {
	DMA_NULL_TYPE = -1,
	DMA_SGL_TYPE = 1,
	DMA_BUFF_TYPE = 2,
};

struct buff_mgr_handle {
	struct dma_pool *mlli_buffs_pool;
};

union buffer_array_entry {
	struct scatterlist *sgl;
	dma_addr_t buffer_dma;
};

struct buffer_array {
	int num_of_buffers;
	union buffer_array_entry entry[MAX_NUM_OF_BUFFERS_IN_MLLI];
	int nents[MAX_NUM_OF_BUFFERS_IN_MLLI];
	int total_data_len[MAX_NUM_OF_BUFFERS_IN_MLLI];
	enum dma_buffer_type type[MAX_NUM_OF_BUFFERS_IN_MLLI];
	bool is_last[MAX_NUM_OF_BUFFERS_IN_MLLI];
};

/**
 * dx_buffer_mgr_get_sgl_nents() - Get scatterlist number of entries.
 * 
 * @sg_list: SG list
 * @nbytes: [IN] Total SGL data bytes.
 * @lbytes: [OUT] Returns the amount of bytes at the last entry 
 */
static int dx_buffer_mgr_get_sgl_nents(
	struct scatterlist *sg_list, int nbytes, int *lbytes, bool *is_chained)
{
	int nents = 0;
	while (nbytes > 0) {
		if (sg_is_chain(sg_list)) {
			DX_LOG_ERR("Unexpected chanined entry "
				   "in sg (entry =0x%X) \n", nents);
			BUG();
		}
		if (sg_list->length != 0) {
			nents++;
			/* get the number of bytes in the last entry */
			*lbytes = nbytes;
			nbytes -= sg_list->length;
			sg_list = sg_next(sg_list);
		} else {
			sg_list = (struct scatterlist *)sg_page(sg_list);
			if (is_chained != NULL) {
				*is_chained = true;
			}
		}
	}
	DX_LOG_DEBUG("nents %d last bytes %d\n",nents, *lbytes);
	return nents;
}

/**
 * dx_buffer_mgr_copy_scatterlist_portion() - Copy scatter list data,
 * from to_skip to end, to dest and vice versa
 * 
 * @dest:
 * @sg:
 * @to_skip:
 * @end:
 * @direct:
 */
void dx_buffer_mgr_copy_scatterlist_portion(
	u8 *dest, struct scatterlist *sg,
	int to_skip, unsigned int end,
	enum dx_sg_cpy_direct direct)
{
	struct scatterlist t_sg;
	struct scatterlist *current_sg = sg;
	int sg_index, cpy_index;
	int nents;
	int lbytes;

	nents = dx_buffer_mgr_get_sgl_nents(sg, end, &lbytes, NULL);
	sg_index = current_sg->length;
	while (sg_index <= to_skip) {
		current_sg = scatterwalk_sg_next(current_sg);
		sg_index += current_sg->length;
		nents--;
	}
	cpy_index = sg_index - to_skip;
	/* copy current sg to temporary */
	t_sg = *current_sg;
	/*update the offset in the sg entry*/
	t_sg.offset += current_sg->length - cpy_index;
	/*copy the data*/
	if (direct == DX_SG_TO_BUF) {
		sg_copy_to_buffer(&t_sg, 1, dest, cpy_index);
	} else {
		sg_copy_from_buffer(&t_sg, 1, dest, cpy_index);
	}
	current_sg = scatterwalk_sg_next(current_sg);
	nents--;

	if (end > sg_index) {
		if (direct == DX_SG_TO_BUF) {
			sg_copy_to_buffer(current_sg, nents,
					  &dest[cpy_index], end - sg_index);
		} else {
			sg_copy_from_buffer(current_sg, nents,
					    &dest[cpy_index], end - sg_index);
		}
	}
}

static inline int dx_buffer_mgr_render_scatterlist_to_mlli(
	struct scatterlist *sgl, uint32_t sgl_data_len,
	uint32_t **mlli_entry_pp)
{
	struct scatterlist *curr_sgl = sgl;
	uint32_t *mlli_entry_p = *mlli_entry_pp;
	int nents = 0;

	for ( ; (curr_sgl != NULL) && (sgl_data_len != 0);
	      curr_sgl = scatterwalk_sg_next(curr_sgl), mlli_entry_p += 2) {
		uint32_t entry_data_len =
			(sgl_data_len > sg_dma_len(curr_sgl)) ?
				sg_dma_len(curr_sgl) : sgl_data_len;
		sgl_data_len -= entry_data_len;
		mlli_entry_p[SEP_LLI_ADDR_WORD_OFFSET] = sg_dma_address(curr_sgl);
		mlli_entry_p[SEP_LLI_SIZE_WORD_OFFSET] = entry_data_len;
		DX_LOG_DEBUG("entry[%d]: addr=0x%08X size=0x%08X\n", nents,
			mlli_entry_p[SEP_LLI_ADDR_WORD_OFFSET],
			mlli_entry_p[SEP_LLI_SIZE_WORD_OFFSET]);
		nents++;
	}
	*mlli_entry_pp = mlli_entry_p;

	return nents;
}

static inline int dx_buffer_mgr_render_buff_to_mlli(
	dma_addr_t buff_dma, uint32_t buff_size,
	uint32_t **mlli_entry_pp)
{
	uint32_t *mlli_entry_p = *mlli_entry_pp;

	mlli_entry_p[SEP_LLI_ADDR_WORD_OFFSET] = buff_dma;
	mlli_entry_p[SEP_LLI_SIZE_WORD_OFFSET] = buff_size;
	DX_LOG_DEBUG("entry[0]: single_buff=0x%08X size=%08X\n",
		   mlli_entry_p[SEP_LLI_ADDR_WORD_OFFSET],
		   mlli_entry_p[SEP_LLI_SIZE_WORD_OFFSET]);
	*mlli_entry_pp = (mlli_entry_p + 2);
	return 1;
}

static int dx_buffer_mgr_generate_mlli(
	struct device *dev,
	struct buffer_array *sg_data,
	struct mlli_params *mlli_params)
{
	uint32_t *mlli_p;
	uint32_t curr_nents = 0;
	int rc = 0, i;

	DX_LOG_DEBUG("NUM of SG's = %d\n", sg_data->num_of_buffers);

	/* Allocate memory from the pointed pool */
	mlli_params->mlli_virt_addr = dma_pool_alloc(
			mlli_params->curr_pool, GFP_KERNEL,
			&(mlli_params->mlli_dma_addr));
	if (unlikely(mlli_params->mlli_virt_addr == NULL)) {
		DX_LOG_ERR("dma_pool_alloc() failed\n");
		rc =-ENOMEM;
		goto build_mlli_exit;
	}

	/* Point to start of MLLI */
	mlli_p = (uint32_t *)mlli_params->mlli_virt_addr;

	/* go over all SG's and link it to one MLLI table */
	for (i = 0; i < sg_data->num_of_buffers; i++) {
		if (sg_data->type[i] == DMA_SGL_TYPE)
			curr_nents += dx_buffer_mgr_render_scatterlist_to_mlli(
				sg_data->entry[i].sgl, sg_data->total_data_len[i],
				&mlli_p);
		else /*DMA_BUFF_TYPE*/
			curr_nents += dx_buffer_mgr_render_buff_to_mlli(
				sg_data->entry[i].buffer_dma,
				sg_data->total_data_len[i],
				&mlli_p);

		/* set last bit in the current table */
		SEP_LLI_SET(&mlli_params->mlli_virt_addr[
			SEP_LLI_ENTRY_BYTE_SIZE * (curr_nents - 1)], 
			LAST, sg_data->is_last[i]);
	}

	/* Set MLLI size */
	mlli_params->mlli_len = (curr_nents * SEP_LLI_ENTRY_BYTE_SIZE);
	SEP_LLI_SET(&mlli_params->mlli_virt_addr[
		SEP_LLI_ENTRY_BYTE_SIZE * (curr_nents - 1)], LAST, 1);

	DX_LOG_DEBUG("MLLI params: "
		     "virt_addr=%pK dma_addr=0x%llX mlli_len=0x%X\n",
		   mlli_params->mlli_virt_addr,
		   (unsigned long long)mlli_params->mlli_dma_addr,
		   mlli_params->mlli_len);

build_mlli_exit:
	return rc;
}

static inline void dx_buffer_mgr_add_buffer_entry(
	struct buffer_array *sgl_data,
	dma_addr_t buffer_dma, unsigned int buffer_len,
	bool is_last_entry)
{
	unsigned int index = sgl_data->num_of_buffers;

	DX_LOG_DEBUG("index=%u single_buff=0x%llX "
		     "buffer_len=0x%08X is_last=%d\n",
		     index, (unsigned long long)buffer_dma, buffer_len, is_last_entry);
	sgl_data->nents[index] = 1;
	sgl_data->entry[index].buffer_dma = buffer_dma;
	sgl_data->total_data_len[index] = buffer_len;
	sgl_data->type[index] = DMA_BUFF_TYPE;
	sgl_data->is_last[index] = is_last_entry;
	sgl_data->num_of_buffers++;
}

static inline void dx_buffer_mgr_add_scatterlist_entry(
	struct buffer_array *sgl_data,
	unsigned int nents,
	struct scatterlist *sgl,
	unsigned int data_len,
	bool is_last_table)
{
	unsigned int index = sgl_data->num_of_buffers;

	DX_LOG_DEBUG("index=%u nents=%u sgl=%pK data_len=0x%08X is_last=%d\n",
		     index, nents, sgl, data_len, is_last_table);
	sgl_data->nents[index] = nents;
	sgl_data->entry[index].sgl = sgl;
	sgl_data->total_data_len[index] = data_len;
	sgl_data->type[index] = DMA_SGL_TYPE;
	sgl_data->is_last[index] = is_last_table;
	sgl_data->num_of_buffers++;
}

static int
dx_buffer_mgr_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
			 enum dma_data_direction direction)
{
	uint32_t i , j;
	struct scatterlist *l_sg = sg;
	for (i = 0; i < nents; i++) {
		if (unlikely(dma_map_sg(dev, l_sg, 1, direction) != 1)){
			DX_LOG_ERR("dma_map_page() sg buffer failed\n");
			goto err;
		}
		l_sg = scatterwalk_sg_next(l_sg);
	}
	return nents;

err:
	/* Restore mapped parts */
	for (j = 0; j < i; j++) {
		dma_unmap_sg(dev,sg,1,direction);
		sg = scatterwalk_sg_next(sg);
	}
	return 0;
}

static int dx_buffer_mgr_map_scatterlist(
	struct device *dev, struct scatterlist *sg,
	unsigned int nbytes, int direction,
	uint32_t *nents, uint32_t max_sg_nents,
	int *lbytes)
{
	bool is_chained = false;

	if (sg_is_last(sg)) {
		/* One entry only case -set to DLLI */
		if (unlikely(dma_map_sg(dev, sg, 1, direction) != 1)) {
			DX_LOG_ERR("dma_map_sg() single buffer failed\n");
			return -ENOMEM;
		} 
		DX_LOG_DEBUG("Mapped sg: dma_address=0x%llX "
			     "page_link=0x%08lX addr=%pK offset=%u "
			     "length=%u\n",
			     (unsigned long long)sg_dma_address(sg), 
			     sg->page_link, 
			     sg_virt(sg), 
			     sg->offset, sg->length);
		*lbytes = nbytes;
		*nents = 1;
	} else {  /*sg_is_last*/
		*nents = dx_buffer_mgr_get_sgl_nents(sg, nbytes, lbytes, 
						     &is_chained);
		if (*nents > max_sg_nents) {
			DX_LOG_ERR("Too many fragments. current %d max %d\n",
				   *nents, max_sg_nents);
			return -ENOMEM;
		}
		if (!is_chained) {
			if (unlikely(dma_map_sg(dev, sg, *nents, direction) 
				     != *nents)){
				DX_LOG_ERR("dma_map_sg() sg buffer failed\n");
				return -ENOMEM;
			}
		} else {
			if (unlikely(dx_buffer_mgr_dma_map_sg(dev, sg, *nents, direction) 
				     != *nents)){
				DX_LOG_ERR("dma_map_sg() sg buffer failed\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

static inline int
dx_aead_handle_config_buf(struct device *dev,
	struct aead_req_ctx *areq_ctx,
	uint8_t* config_data,
	struct buffer_array *sg_data,
	unsigned int assoclen)
{
	DX_LOG_DEBUG(" handle additional data config set to   DLLI \n");
	/* create sg for the current buffer */
	sg_init_one(&areq_ctx->ccm_adata_sg, config_data, AES_BLOCK_SIZE + areq_ctx->ccm_hdr_size);
	if (unlikely(dma_map_sg(dev, &areq_ctx->ccm_adata_sg, 1, 
				DMA_TO_DEVICE) != 1)) {
			DX_LOG_ERR("dma_map_sg() "
			   "config buffer failed\n");
			return -ENOMEM;
	}
	DX_LOG_DEBUG("Mapped curr_buff: dma_address=0x%llX "
		     "page_link=0x%08lX addr=%pK "
		     "offset=%u length=%u\n",
		     (unsigned long long)sg_dma_address(&areq_ctx->ccm_adata_sg), 
		     areq_ctx->ccm_adata_sg.page_link, 
		     sg_virt(&areq_ctx->ccm_adata_sg),
		     areq_ctx->ccm_adata_sg.offset, 
		     areq_ctx->ccm_adata_sg.length);
	/* prepare for case of MLLI */
	if (assoclen > 0) {
		dx_buffer_mgr_add_scatterlist_entry(sg_data, 1, &areq_ctx->ccm_adata_sg,
					AES_BLOCK_SIZE + areq_ctx->ccm_hdr_size, false);
	}
	return 0;
}


static inline int dx_ahash_handle_curr_buf(struct device *dev,
					   struct ahash_req_ctx *areq_ctx,
					   uint8_t* curr_buff,
					   uint32_t curr_buff_cnt,
					   struct buffer_array *sg_data)
{
	DX_LOG_DEBUG(" handle curr buff %x set to   DLLI \n", curr_buff_cnt);
	/* create sg for the current buffer */
	sg_init_one(areq_ctx->buff_sg,curr_buff, curr_buff_cnt);
	if (unlikely(dma_map_sg(dev, areq_ctx->buff_sg, 1,
				DMA_TO_DEVICE) != 1)) {
			DX_LOG_ERR("dma_map_sg() "
			   "src buffer failed\n");
			return -ENOMEM;
	}
	DX_LOG_DEBUG("Mapped curr_buff: dma_address=0x%llX "
		     "page_link=0x%08lX addr=%pK "
		     "offset=%u length=%u\n",
		     (unsigned long long)sg_dma_address(areq_ctx->buff_sg), 
		     areq_ctx->buff_sg->page_link, 
		     sg_virt(areq_ctx->buff_sg),
		     areq_ctx->buff_sg->offset, 
		     areq_ctx->buff_sg->length);
	areq_ctx->data_dma_buf_type = DX_DMA_BUF_DLLI;
	areq_ctx->curr_sg = areq_ctx->buff_sg;
	areq_ctx->in_nents = 0;
	/* prepare for case of MLLI */
	dx_buffer_mgr_add_scatterlist_entry(sg_data, 1, areq_ctx->buff_sg,
				curr_buff_cnt, false);
	return 0;
}

void dx_buffer_mgr_unmap_ablkcipher_request(
	struct device *dev, struct ablkcipher_request *req)
{
	struct ablkcipher_req_ctx *areq_ctx = ablkcipher_request_ctx(req);
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	unsigned int iv_size = crypto_ablkcipher_ivsize(tfm);

	if (likely(areq_ctx->gen_ctx.iv_dma_addr != 0)) {
		DX_LOG_DEBUG("Unmapped iv: iv_dma_addr=0x%llX iv_size=%u\n", 
			(unsigned long long)areq_ctx->gen_ctx.iv_dma_addr,
			iv_size);
		dma_unmap_single(dev, areq_ctx->gen_ctx.iv_dma_addr, 
				 iv_size, 
				 areq_ctx->is_giv ? DMA_BIDIRECTIONAL :
				 DMA_TO_DEVICE);
	}
	/* Release pool */
	if (areq_ctx->dma_buf_type == DX_DMA_BUF_MLLI)
		dma_pool_free(areq_ctx->mlli_params.curr_pool,
			      areq_ctx->mlli_params.mlli_virt_addr,
			      areq_ctx->mlli_params.mlli_dma_addr);

	if (areq_ctx->sec_dir != DX_SRC_DMA_IS_SECURE) {
		dma_unmap_sg(dev, req->src, areq_ctx->in_nents,
			DMA_BIDIRECTIONAL);
		DX_LOG_DEBUG("Unmapped req->src=%pK\n", 
			     sg_virt(req->src));
	}

	if (req->src != req->dst) {
		if (areq_ctx->sec_dir != DX_DST_DMA_IS_SECURE) {
			dma_unmap_sg(dev, req->dst, areq_ctx->out_nents, 
				DMA_BIDIRECTIONAL);
			DX_LOG_DEBUG("Unmapped req->dst=%pK\n",
				sg_virt(req->dst));
		}
	}
}

int dx_buffer_mgr_map_ablkcipher_request(
	struct dx_drvdata *drvdata, struct ablkcipher_request *req)
{
	struct ablkcipher_req_ctx *areq_ctx = ablkcipher_request_ctx(req);
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	unsigned int iv_size = crypto_ablkcipher_ivsize(tfm);
	struct mlli_params *mlli_params = &areq_ctx->mlli_params;	
	struct buff_mgr_handle *buff_mgr = drvdata->buff_mgr_handle;
	struct device *dev = &drvdata->plat_dev->dev;
	struct buffer_array sg_data;
	int dummy = 0;
	int rc = 0;

	areq_ctx->sec_dir = 0;
	areq_ctx->dma_buf_type = DX_DMA_BUF_DLLI;
	mlli_params->curr_pool = NULL;
	sg_data.num_of_buffers = 0;

	/* Map IV buffer */
	if (likely(iv_size != 0) ) {
		dump_byte_array("iv", (uint8_t *)req->info, iv_size);
		areq_ctx->gen_ctx.iv_dma_addr = 
			dma_map_single(dev, (void *)req->info, 
				       iv_size, 
				       areq_ctx->is_giv ? DMA_BIDIRECTIONAL:
				       DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(dev, 
					areq_ctx->gen_ctx.iv_dma_addr))) {
			DX_LOG_ERR("Mapping iv %u B at va=%pK "
				   "for DMA failed\n", iv_size, req->info);
			return -ENOMEM;
		}
		DX_LOG_DEBUG("Mapped iv %u B at va=%pK to dma=0x%llX\n",
			iv_size, req->info,
			(unsigned long long)areq_ctx->gen_ctx.iv_dma_addr);
	} else
		areq_ctx->gen_ctx.iv_dma_addr = 0;
	
	/* Map the src SGL */
	if (sg_is_last(req->src) &&
	    (sg_page(req->src) == NULL) &&
	     sg_dma_address(req->src)) {
		/* The source is secure hence, no mapping is needed */
		areq_ctx->sec_dir = DX_SRC_DMA_IS_SECURE;
		areq_ctx->in_nents = 1;
	} else {
		rc = dx_buffer_mgr_map_scatterlist(dev,req->src,
			req->nbytes, DMA_BIDIRECTIONAL, &areq_ctx->in_nents,
			LLI_MAX_NUM_OF_DATA_ENTRIES,&dummy);
		if (unlikely(rc != 0)) {
			rc = -ENOMEM;
			goto ablkcipher_exit;
		}
		if (areq_ctx->in_nents > 1)
			areq_ctx->dma_buf_type = DX_DMA_BUF_MLLI;
	}

	if (unlikely(req->src == req->dst)) {
		if (areq_ctx->sec_dir == DX_SRC_DMA_IS_SECURE) {
			DX_LOG_ERR("Inplace operation for Secure key "
				   "is un-supported\n");
			/* both sides are secure */
			rc = -ENOMEM;
			goto ablkcipher_exit;
		}
		/* Handle inplace operation */
		if (unlikely(areq_ctx->dma_buf_type == DX_DMA_BUF_MLLI)) {
			areq_ctx->out_nents = 0;
			dx_buffer_mgr_add_scatterlist_entry(&sg_data,
				areq_ctx->in_nents, req->src,
				req->nbytes, true);
		}
	} else {
		if (sg_is_last(req->dst) && (sg_page(req->dst) == NULL) &&
		    sg_dma_address(req->dst)) {
			if (areq_ctx->sec_dir == DX_SRC_DMA_IS_SECURE) {
				DX_LOG_ERR("Secure key in both sides is"
					   "un-supported \n");
				/* both sides are secure */
				rc = -ENOMEM;
				goto ablkcipher_exit;
			}
			/* The dest is secure no mapping is needed */
			areq_ctx->sec_dir = DX_DST_DMA_IS_SECURE;
			areq_ctx->out_nents = 1;
		} else {
			/* Map the dst sg */
			if (unlikely(dx_buffer_mgr_map_scatterlist(
				dev,req->dst, req->nbytes,
				DMA_BIDIRECTIONAL, &areq_ctx->out_nents,
				LLI_MAX_NUM_OF_DATA_ENTRIES, &dummy))){
				rc = -ENOMEM;
				goto ablkcipher_exit;
			}
			if (areq_ctx->out_nents > 1)
				areq_ctx->dma_buf_type = DX_DMA_BUF_MLLI;
		}
		if (unlikely((areq_ctx->dma_buf_type == DX_DMA_BUF_MLLI))) {
			if (areq_ctx->sec_dir != DX_SRC_DMA_IS_SECURE)
				dx_buffer_mgr_add_scatterlist_entry(&sg_data,
					areq_ctx->in_nents, req->src,
					req->nbytes, true);
			if (areq_ctx->sec_dir != DX_DST_DMA_IS_SECURE)
				dx_buffer_mgr_add_scatterlist_entry(&sg_data,
					areq_ctx->out_nents, req->dst,
					req->nbytes, true);
		}
	}
	
	if (unlikely(areq_ctx->dma_buf_type == DX_DMA_BUF_MLLI)) {
		mlli_params->curr_pool = buff_mgr->mlli_buffs_pool;
		rc = dx_buffer_mgr_generate_mlli(dev, &sg_data, mlli_params);
		if (unlikely(rc!= 0))
			goto ablkcipher_exit;

	}

	DX_LOG_DEBUG("areq_ctx->dma_buf_type = %s\n",
		GET_DMA_BUFFER_TYPE(areq_ctx->dma_buf_type));

	return 0;

ablkcipher_exit:
	dx_buffer_mgr_unmap_ablkcipher_request(dev, req);
	return rc;
}

void dx_buffer_mgr_unmap_aead_request(
	struct device *dev, struct aead_request *req)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	unsigned int hw_iv_size = areq_ctx->hw_iv_size;
	
	if (areq_ctx->mac_buf_dma_addr != 0)
		dma_unmap_single(dev, areq_ctx->mac_buf_dma_addr, 
			MAX_MAC_SIZE, DMA_BIDIRECTIONAL);

	if (areq_ctx->ccm_hdr_size != ccm_header_size_null) {
		if (areq_ctx->ccm_iv0_dma_addr != 0)
			dma_unmap_single(dev, areq_ctx->ccm_iv0_dma_addr, 
				AES_BLOCK_SIZE, DMA_TO_DEVICE);
		if (&areq_ctx->ccm_adata_sg != NULL)
			dma_unmap_sg(dev, &areq_ctx->ccm_adata_sg,
				1, DMA_TO_DEVICE);
	}
	if (areq_ctx->gen_ctx.iv_dma_addr != 0)
		dma_unmap_single(dev, areq_ctx->gen_ctx.iv_dma_addr,
				 hw_iv_size, DMA_BIDIRECTIONAL);

	/*In case a pool was set, a table was 
	  allocated and should be released */
	if (areq_ctx->mlli_params.curr_pool != NULL) {
		DX_LOG_DEBUG("free MLLI buffer: dma=0x%08llX virt=%pK\n", 
			(unsigned long long)areq_ctx->mlli_params.mlli_dma_addr,
			areq_ctx->mlli_params.mlli_virt_addr);
		dma_pool_free(areq_ctx->mlli_params.curr_pool,
			      areq_ctx->mlli_params.mlli_virt_addr,
			      areq_ctx->mlli_params.mlli_dma_addr);
	}
	if (areq_ctx->assoc_buff_type != DX_DMA_BUF_NULL) {
		DX_LOG_DEBUG("Unmapping sgl assoc: req->assoc=%pK\n", 
			sg_virt(req->assoc));
		dma_unmap_sg(dev, req->assoc, areq_ctx->assoc.nents,
			DMA_TO_DEVICE);
	}

	DX_LOG_DEBUG("Unmapping src sgl: req->src=%pK\n", sg_virt(req->src));
	dma_unmap_sg(dev, req->src, areq_ctx->src.nents, DMA_BIDIRECTIONAL);
	if (unlikely(req->src != req->dst)) {
		DX_LOG_DEBUG("Unmapping dst sgl: req->dst=%pK\n", 
			sg_virt(req->dst));
		dma_unmap_sg(dev, req->dst, areq_ctx->dst.nents,
			DMA_BIDIRECTIONAL);
	}

#if DX_HAS_ACP
	if ((areq_ctx->gen_ctx.op_type == SEP_CRYPTO_DIRECTION_DECRYPT) &&
	    likely(req->src == req->dst))
		/* re-copy saved mac to it's original location
		   to deal with possible data memory overriding that
		   caused by cache coherence problem. */
		dx_buffer_mgr_copy_scatterlist_portion(areq_ctx->backup_mac,
			req->src, req->cryptlen - areq_ctx->req_authsize,
			req->cryptlen, DX_SG_FROM_BUF);
#endif
}

static inline int dx_buffer_mgr_get_aead_icv_nents(
	struct scatterlist *sgl,
	unsigned int sgl_nents,
	unsigned int authsize,
	int last_entry_data_size,
	bool *is_icv_fragmented)
{
	unsigned int icv_max_size;
	unsigned int icv_required_size = (authsize - last_entry_data_size);
	unsigned int nents;
	unsigned int i;
	
	if (sgl_nents < MAX_ICV_NENTS_SUPPORTED) {
		*is_icv_fragmented = false;
		return 0;
	}
	
	for( i = 0 ; i < (sgl_nents - MAX_ICV_NENTS_SUPPORTED) ; i++) {
		sgl = scatterwalk_sg_next(sgl);
	}
	icv_max_size = sgl->length;

	if (last_entry_data_size > authsize) {
		nents = 0; /* ICV attached to data in last entry (not fragmented!) */
		*is_icv_fragmented = false;
	} else if (last_entry_data_size == authsize) {
		nents = 1; /* ICV placed in whole last entry (not fragmented!) */
		*is_icv_fragmented = false;
	} else if (icv_max_size > icv_required_size) {
		nents = 1;
		*is_icv_fragmented = true;
	} else if (icv_max_size == icv_required_size) {
		nents = 2;
		*is_icv_fragmented = true;
	} else {
		DX_LOG_ERR("Unsupported num. of ICV fragments (> %d)\n",
			MAX_ICV_NENTS_SUPPORTED);
		nents = -1; /*unsupported*/
	}
	DX_LOG_DEBUG("is_frag=%s icv_nents=%u\n",
		(*is_icv_fragmented ? "true" : "false"), nents);

	return nents;
}

static inline int dx_buffer_mgr_aead_chain_iv(
	struct dx_drvdata *drvdata,
	struct aead_request *req,
	struct buffer_array *sg_data,
	bool is_last, bool do_chain)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	unsigned int hw_iv_size = areq_ctx->hw_iv_size;
	struct device *dev = &drvdata->plat_dev->dev;
	int rc = 0;

	if (unlikely(req->iv == NULL)) {
		areq_ctx->gen_ctx.iv_dma_addr = 0;
		goto chain_iv_exit;
	}

	dump_byte_array("iv", (uint8_t *)req->iv, hw_iv_size);

	areq_ctx->gen_ctx.iv_dma_addr = dma_map_single(dev, req->iv,
		hw_iv_size, DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, areq_ctx->gen_ctx.iv_dma_addr))) {
		DX_LOG_ERR("Mapping iv %u B at va=%pK for DMA failed\n",
			hw_iv_size, req->iv);
		rc = -ENOMEM;
		goto chain_iv_exit; 
	}
	DX_LOG_DEBUG("Mapped iv %u B at va=%pK to dma=0x%llX\n",
		hw_iv_size, req->iv, 
		(unsigned long long)areq_ctx->gen_ctx.iv_dma_addr);

	if (do_chain == true) {
		struct crypto_aead *tfm = crypto_aead_reqtfm(req);
		unsigned int iv_size_to_authenc = crypto_aead_ivsize(tfm);
		unsigned int iv_ofs = 0;

		if (areq_ctx->cipher_mode == SEP_CIPHER_CTR) {
			iv_ofs = CTR_RFC3686_NONCE_SIZE;
		}

		/* Chain to given list */
		dx_buffer_mgr_add_buffer_entry(
			sg_data, areq_ctx->gen_ctx.iv_dma_addr + iv_ofs,
			iv_size_to_authenc, is_last);

		if (areq_ctx->assoc.sram_addr == NULL_SRAM_ADDR) {
			/* No assoc. data, hence mlli table starts from IV freg. */
			areq_ctx->assoc.sram_addr =
				drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		}
		areq_ctx->curr_mlli_size += SEP_LLI_ENTRY_BYTE_SIZE;

		DX_LOG_DEBUG("iv: dma_addr=0x%llX entry_size=%zu\n",
			(unsigned long long)areq_ctx->gen_ctx.iv_dma_addr,
			SEP_LLI_ENTRY_BYTE_SIZE);
	}

chain_iv_exit:
	return rc;
}

static inline int dx_buffer_mgr_aead_chain_assoc(
	struct dx_drvdata *drvdata,
	struct aead_request *req,
	struct buffer_array *sg_data,
	bool is_last, bool do_chain)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct device *dev = &drvdata->plat_dev->dev;
	int dummy = 0; /*used for the assoc data fragments */
	int rc = 0;

	if (sg_data == NULL) {
		rc = -EINVAL;
		goto chain_assoc_exit;
	}

	if (unlikely(req->assoclen == 0)) {
		areq_ctx->assoc_buff_type = DX_DMA_BUF_NULL;
		areq_ctx->assoc.nents = 0;
		areq_ctx->assoc.sram_addr = NULL_SRAM_ADDR;
		goto chain_assoc_exit;
	}

	rc = dx_buffer_mgr_map_scatterlist(dev, req->assoc,
		 req->assoclen, DMA_TO_DEVICE, &(areq_ctx->assoc.nents),
		 LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES, &dummy);
	if (unlikely(rc != 0)) {
		rc = -ENOMEM;
		goto chain_assoc_exit; 
	}

	/* in CCM case we have additional entry for
	*  ccm header configurations */
	if (areq_ctx->ccm_hdr_size != ccm_header_size_null) {
		if (unlikely((areq_ctx->assoc.nents + 1) >
			LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES)) {

			DX_LOG_ERR("CCM case.Too many fragments. "
				"Current %d max %d\n",
				(areq_ctx->assoc.nents + 1),
				LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES);
			rc = -ENOMEM;
			goto chain_assoc_exit;
		}
	}

	if (likely(areq_ctx->assoc.nents == 1) &&
	    (areq_ctx->ccm_hdr_size == ccm_header_size_null))
		areq_ctx->assoc_buff_type = DX_DMA_BUF_DLLI;
	else
		areq_ctx->assoc_buff_type = DX_DMA_BUF_MLLI;

	if (unlikely((do_chain == true) ||
		(areq_ctx->assoc_buff_type == DX_DMA_BUF_MLLI))) {
		uint32_t assoc_mlli_size;

		DX_LOG_DEBUG("Chain assoc: buff_type=%s nents=%u\n",
			GET_DMA_BUFFER_TYPE(areq_ctx->assoc_buff_type),
			areq_ctx->assoc.nents);

		dx_buffer_mgr_add_scatterlist_entry(
			sg_data, areq_ctx->assoc.nents,
			req->assoc, req->assoclen, is_last);

		/* store assoc mlli address in SRAM */
		areq_ctx->assoc.sram_addr =
			drvdata->mlli_sram_addr +
			areq_ctx->curr_mlli_size;

		assoc_mlli_size = areq_ctx->assoc.nents * SEP_LLI_ENTRY_BYTE_SIZE;
		if (areq_ctx->ccm_hdr_size != ccm_header_size_null) {
			/*we have additional entry of configutation buffer*/
			assoc_mlli_size += SEP_LLI_ENTRY_BYTE_SIZE;
		}

		areq_ctx->curr_mlli_size += assoc_mlli_size;
		DX_LOG_DEBUG("assoc: mlli_addr=%08x mlli_size=%08x\n",
			areq_ctx->assoc.sram_addr, assoc_mlli_size);
	}

chain_assoc_exit:
	return rc;
}

static inline void dx_buffer_mgr_prepare_aead_data_dlli(
	struct aead_request *req,
	int *src_last_bytes, int *dst_last_bytes)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	enum sep_crypto_direction direct = areq_ctx->gen_ctx.op_type;
	unsigned int authsize = areq_ctx->req_authsize;

	areq_ctx->is_icv_fragmented = false;
	if (likely(req->src == req->dst)) {
		/*INPLACE*/
		areq_ctx->icv_dma_addr = sg_dma_address(
			&req->src[0]) +
			(*src_last_bytes - authsize);
		areq_ctx->icv_virt_addr = sg_virt(
			&req->src[0]) +
			(*src_last_bytes - authsize);
	} else if (direct == SEP_CRYPTO_DIRECTION_DECRYPT) {
		/*NON-INPLACE and DECRYPT*/
		areq_ctx->icv_dma_addr = sg_dma_address(
			&req->src[0]) +
			(*src_last_bytes - authsize);
		areq_ctx->icv_virt_addr = sg_virt(
			&req->src[0]) +
			(*src_last_bytes - authsize);
	} else {
		/*NON-INPLACE and ENCRYPT*/
		areq_ctx->icv_dma_addr = sg_dma_address(
			&req->dst[0]) +
			(*dst_last_bytes - authsize);
		areq_ctx->icv_virt_addr = sg_virt(
			&req->dst[0]) +
			(*dst_last_bytes - authsize);
	}
}

static inline int dx_buffer_mgr_prepare_aead_data_mlli(
	struct dx_drvdata *drvdata,
	struct aead_request *req,
	struct buffer_array *sg_data,
	int *src_last_bytes, int *dst_last_bytes,
	bool is_last_table)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	enum sep_crypto_direction direct = areq_ctx->gen_ctx.op_type;
	unsigned int authsize = areq_ctx->req_authsize;
	uint32_t dst_mlli_size, src_mlli_size;
	int rc = 0, icv_nents;

	if (likely(req->src == req->dst)) {
		/*INPLACE*/
		dx_buffer_mgr_add_scatterlist_entry(sg_data,
			areq_ctx->src.nents, req->src,
			areq_ctx->cryptlen, is_last_table);

		icv_nents = dx_buffer_mgr_get_aead_icv_nents(req->src,
			areq_ctx->src.nents, authsize, *src_last_bytes,
			&areq_ctx->is_icv_fragmented);
		if (unlikely(icv_nents < 0)) {
			rc = -ENOTSUPP;
			goto prepare_data_mlli_exit;
		}

		if (unlikely(areq_ctx->is_icv_fragmented == true)) {
			/* Backup happens only when ICV is fragmented, ICV
			   verification is made by CPU compare in order to simplify
			   MAC verification upon request completion */
			if (direct == SEP_CRYPTO_DIRECTION_DECRYPT) {
#if !DX_HAS_ACP
				/* In ACP platform we already copying ICV
				   for any INPLACE-DECRYPT operation, hence
				   we must neglect this code. */
				dx_buffer_mgr_copy_scatterlist_portion(
					areq_ctx->backup_mac, req->src,
					req->cryptlen - authsize,
					req->cryptlen, DX_SG_TO_BUF);
#endif
				areq_ctx->icv_virt_addr = areq_ctx->backup_mac;
			} else {
				areq_ctx->icv_virt_addr = areq_ctx->mac_buf;
				areq_ctx->icv_dma_addr = areq_ctx->mac_buf_dma_addr;
			}
		} else { /* Contig. ICV */
			/*Should hanlde if the sg is not contig.*/
			areq_ctx->icv_dma_addr = sg_dma_address(
				&req->src[areq_ctx->src.nents - 1]) +
				(*src_last_bytes - authsize);
			areq_ctx->icv_virt_addr = sg_virt(
				&req->src[areq_ctx->src.nents - 1]) +
				(*src_last_bytes - authsize);
		}

		src_mlli_size = (areq_ctx->src.nents + icv_nents) *
				SEP_LLI_ENTRY_BYTE_SIZE;
		areq_ctx->src.sram_addr = drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		areq_ctx->curr_mlli_size += src_mlli_size;
		areq_ctx->dst = areq_ctx->src;
		DX_LOG_DEBUG("src_mlli=dst_mlli addr=%08x size=%08x\n",
			areq_ctx->src.sram_addr, src_mlli_size);
	} else if (direct == SEP_CRYPTO_DIRECTION_DECRYPT) {
		/*NON-INPLACE and DECRYPT*/
		dx_buffer_mgr_add_scatterlist_entry(sg_data,
			areq_ctx->src.nents, req->src,
			areq_ctx->cryptlen, is_last_table);
		dx_buffer_mgr_add_scatterlist_entry(sg_data,
			areq_ctx->dst.nents, req->dst,
			areq_ctx->cryptlen, is_last_table);

		icv_nents = dx_buffer_mgr_get_aead_icv_nents(req->src,
			areq_ctx->src.nents, authsize, *src_last_bytes,
			&areq_ctx->is_icv_fragmented);
		if (unlikely(icv_nents < 0)) {
			rc = -ENOTSUPP;
			goto prepare_data_mlli_exit;
		}

		if (unlikely(areq_ctx->is_icv_fragmented == true)) {
			/* Backup happens only when ICV is fragmented, ICV
			   verification is made by CPU compare in order to simplify
			   MAC verification upon request completion */
			dx_buffer_mgr_copy_scatterlist_portion(
				areq_ctx->backup_mac, req->src,
				req->cryptlen - authsize,
				req->cryptlen, DX_SG_TO_BUF);
			areq_ctx->icv_virt_addr = areq_ctx->backup_mac;
		} else { /* Contig. ICV */
			/*Should hanlde if the sg is not contig.*/
			areq_ctx->icv_dma_addr = sg_dma_address(
				&req->src[areq_ctx->src.nents - 1]) +
				(*src_last_bytes - authsize);
			areq_ctx->icv_virt_addr = sg_virt(
				&req->src[areq_ctx->src.nents - 1]) +
				(*src_last_bytes - authsize);
		}

		src_mlli_size = (areq_ctx->src.nents - icv_nents) *
				SEP_LLI_ENTRY_BYTE_SIZE;
		dst_mlli_size = areq_ctx->dst.nents *
				SEP_LLI_ENTRY_BYTE_SIZE;

		areq_ctx->src.sram_addr = drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		areq_ctx->curr_mlli_size += src_mlli_size;
		DX_LOG_DEBUG("src_mlli addr=%08x size=%08x nents=%u\n",
				areq_ctx->src.sram_addr, src_mlli_size,
				areq_ctx->src.nents);

		areq_ctx->dst.sram_addr = drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		areq_ctx->curr_mlli_size += dst_mlli_size;
		DX_LOG_DEBUG("dst_mlli addr=%08x size=%08x nents=%u\n",
				areq_ctx->dst.sram_addr, dst_mlli_size,
				areq_ctx->dst.nents);
	} else {
		/*NON-INPLACE and ENCRYPT*/
		dx_buffer_mgr_add_scatterlist_entry(sg_data,
			areq_ctx->dst.nents, req->dst,
			areq_ctx->cryptlen, is_last_table);
		dx_buffer_mgr_add_scatterlist_entry(sg_data,
			areq_ctx->src.nents, req->src,
			areq_ctx->cryptlen, is_last_table);

		icv_nents = dx_buffer_mgr_get_aead_icv_nents(req->dst,
			areq_ctx->dst.nents, authsize, *dst_last_bytes,
			&areq_ctx->is_icv_fragmented);
		if (unlikely(icv_nents < 0)) {
			rc = -ENOTSUPP;
			goto prepare_data_mlli_exit;
		}

		if (likely(areq_ctx->is_icv_fragmented == false)) {
			/* Contig. ICV */
			areq_ctx->icv_dma_addr = sg_dma_address(
				&req->dst[areq_ctx->dst.nents - 1]) +
				(*dst_last_bytes - authsize);
			areq_ctx->icv_virt_addr = sg_virt(
				&req->dst[areq_ctx->dst.nents - 1]) +
				(*dst_last_bytes - authsize);
		} else {
			areq_ctx->icv_dma_addr = areq_ctx->mac_buf_dma_addr;
			areq_ctx->icv_virt_addr = areq_ctx->mac_buf;
		}

		dst_mlli_size = (areq_ctx->dst.nents - icv_nents) *
				SEP_LLI_ENTRY_BYTE_SIZE;
		src_mlli_size = areq_ctx->src.nents *
				SEP_LLI_ENTRY_BYTE_SIZE;

		areq_ctx->dst.sram_addr = drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		areq_ctx->curr_mlli_size += dst_mlli_size;
		DX_LOG_DEBUG("dst_mlli _addr=%08x size=%08x nents=%u\n",
				areq_ctx->dst.sram_addr,
				dst_mlli_size, areq_ctx->dst.nents);

		areq_ctx->src.sram_addr =
				drvdata->mlli_sram_addr +
				areq_ctx->curr_mlli_size;
		areq_ctx->curr_mlli_size += src_mlli_size;
		DX_LOG_DEBUG("src_mlli addr=%08x size=%08x nents=%u\n",
				areq_ctx->src.sram_addr,
				src_mlli_size, areq_ctx->src.nents);
	}

prepare_data_mlli_exit:
	return rc;
}

static inline int dx_buffer_mgr_aead_chain_data(
	struct dx_drvdata *drvdata,
	struct aead_request *req,
	struct buffer_array *sg_data,
	bool is_last_table, bool do_chain)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct device *dev = &drvdata->plat_dev->dev;
	enum sep_crypto_direction direct = areq_ctx->gen_ctx.op_type;
	unsigned int size_for_map = req->cryptlen; /*non-inplace mode*/
	unsigned int authsize = areq_ctx->req_authsize;
	int src_last_bytes = 0, dst_last_bytes = 0;
	int rc = 0;

	if (sg_data == NULL) {
		rc = -EINVAL;
		goto chain_data_exit;
	}

	if (likely(req->src == req->dst)) {
		size_for_map = (direct == SEP_CRYPTO_DIRECTION_ENCRYPT) ?
			(req->cryptlen + authsize) : req->cryptlen;
	}

	rc = dx_buffer_mgr_map_scatterlist(dev, req->src, size_for_map,
		DMA_BIDIRECTIONAL, &(areq_ctx->src.nents),
		LLI_MAX_NUM_OF_DATA_ENTRIES, &src_last_bytes);
	if (unlikely(rc != 0)) {
		rc = -ENOMEM;
		goto chain_data_exit;
	}

	if (req->src != req->dst) {
		size_for_map = (direct == SEP_CRYPTO_DIRECTION_ENCRYPT) ?
			(req->cryptlen + authsize) :
			(req->cryptlen - authsize);

		rc = dx_buffer_mgr_map_scatterlist(dev, req->dst, size_for_map,
			 DMA_BIDIRECTIONAL, &(areq_ctx->dst.nents),
			 LLI_MAX_NUM_OF_DATA_ENTRIES, &dst_last_bytes);
		if (unlikely(rc != 0)) {
			rc = -ENOMEM;
			goto chain_data_exit; 
		}
	}

	if ((areq_ctx->src.nents > 1) ||
	    (areq_ctx->dst.nents > 1) ||
	    (do_chain == true)) {
		areq_ctx->data_buff_type = DX_DMA_BUF_MLLI;
		rc = dx_buffer_mgr_prepare_aead_data_mlli(drvdata, req, sg_data,
			&src_last_bytes, &dst_last_bytes, is_last_table);
	} else {
		areq_ctx->data_buff_type = DX_DMA_BUF_DLLI;
		dx_buffer_mgr_prepare_aead_data_dlli(
				req, &src_last_bytes, &dst_last_bytes);
	}

chain_data_exit:
	return rc;
}

int dx_buffer_mgr_map_aead_request(
	struct dx_drvdata *drvdata, struct aead_request *req)
{
	struct aead_req_ctx *areq_ctx = aead_request_ctx(req);
	struct mlli_params *mlli_params = &areq_ctx->mlli_params;
	struct device *dev = &drvdata->plat_dev->dev;
	struct buffer_array sg_data;
	unsigned int authsize = areq_ctx->req_authsize;
	struct buff_mgr_handle *buff_mgr = drvdata->buff_mgr_handle;
	int rc = 0;

	areq_ctx->curr_mlli_size = 0;
	mlli_params->curr_pool = NULL;
	sg_data.num_of_buffers = 0;

#if DX_HAS_ACP
	if ((areq_ctx->gen_ctx.op_type == SEP_CRYPTO_DIRECTION_DECRYPT) &&
	    likely(req->src == req->dst))
		/* copy mac to a temporary location to deal with possible
		   data memory overriding that caused by cache coherence problem. */
		dx_buffer_mgr_copy_scatterlist_portion(
			areq_ctx->backup_mac, req->src,
			req->cryptlen - areq_ctx->req_authsize,
			req->cryptlen, DX_SG_TO_BUF);
#endif

	/* cacluate the size for cipher remove ICV in decrypt*/
	areq_ctx->cryptlen = (areq_ctx->gen_ctx.op_type == 
				 SEP_CRYPTO_DIRECTION_ENCRYPT) ? 
				req->cryptlen :
				(req->cryptlen - authsize);

	areq_ctx->mac_buf_dma_addr = dma_map_single(dev,
		areq_ctx->mac_buf, MAX_MAC_SIZE, DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, areq_ctx->mac_buf_dma_addr))) {
		DX_LOG_ERR("Mapping mac_buf %u B at va=%pK for DMA failed\n",
			MAX_MAC_SIZE, areq_ctx->mac_buf);
		rc = -ENOMEM;
		goto aead_map_failure;
	}

	if (areq_ctx->ccm_hdr_size != ccm_header_size_null) {
		areq_ctx->ccm_iv0_dma_addr = dma_map_single(dev,
			(areq_ctx->ccm_config + CCM_CTR_COUNT_0_OFFSET),
			AES_BLOCK_SIZE, DMA_TO_DEVICE);

		if (unlikely(dma_mapping_error(dev, areq_ctx->ccm_iv0_dma_addr))) {
			DX_LOG_ERR("Mapping mac_buf %u B at va=%pK "
			"for DMA failed\n", AES_BLOCK_SIZE,
			(areq_ctx->ccm_config + CCM_CTR_COUNT_0_OFFSET));
			areq_ctx->ccm_iv0_dma_addr = 0;
			rc = -ENOMEM;
			goto aead_map_failure;
		}
		if (dx_aead_handle_config_buf(dev, areq_ctx,
			areq_ctx->ccm_config, &sg_data, req->assoclen) != 0) {
			rc = -ENOMEM;
			goto aead_map_failure;
		}
	}

	if (likely(areq_ctx->is_single_pass == true)) {
		/*
		* Create MLLI table for: 
		*   (1) Assoc. data
		*   (2) Src/Dst SGLs
		*   Note: IV is contg. buffer (not an SGL) 
		*/
		rc = dx_buffer_mgr_aead_chain_assoc(drvdata, req, &sg_data, true, false);
		if (unlikely(rc != 0))
			goto aead_map_failure;
		rc = dx_buffer_mgr_aead_chain_iv(drvdata, req, &sg_data, true, false);
		if (unlikely(rc != 0))
			goto aead_map_failure;
		rc = dx_buffer_mgr_aead_chain_data(drvdata, req, &sg_data, true, false);
		if (unlikely(rc != 0))
			goto aead_map_failure;
	} else { /* DOUBLE-PASS flow */
		/*
		* Prepare MLLI table(s) in this order:
		*  
		* If ENCRYPT/DECRYPT (inplace):
		*   (1) MLLI table for assoc
		*   (2) IV entry (chained right after end of assoc)
		*   (3) MLLI for src/dst (inplace operation)
		*  
		* If ENCRYPT (non-inplace) 
		*   (1) MLLI table for assoc
		*   (2) IV entry (chained right after end of assoc)
		*   (3) MLLI for dst
		*   (4) MLLI for src
		*  
		* If DECRYPT (non-inplace) 
		*   (1) MLLI table for assoc
		*   (2) IV entry (chained right after end of assoc)
		*   (3) MLLI for src
		*   (4) MLLI for dst
		*/
		rc = dx_buffer_mgr_aead_chain_assoc(drvdata, req, &sg_data, false, true);
		if (unlikely(rc != 0))
			goto aead_map_failure;
		rc = dx_buffer_mgr_aead_chain_iv(drvdata, req, &sg_data, false, true);
		if (unlikely(rc != 0))
			goto aead_map_failure;
		rc = dx_buffer_mgr_aead_chain_data(drvdata, req, &sg_data, true, true);
		if (unlikely(rc != 0))
			goto aead_map_failure;
	}

	/* Mlli support -start building the MLLI according to the above results */
	if (unlikely(
		(areq_ctx->assoc_buff_type == DX_DMA_BUF_MLLI) ||
		(areq_ctx->data_buff_type == DX_DMA_BUF_MLLI))) {

		mlli_params->curr_pool = buff_mgr->mlli_buffs_pool;
		rc = dx_buffer_mgr_generate_mlli(dev, &sg_data, mlli_params);
		if (unlikely(rc != 0)) {
			goto aead_map_failure;
		}
	}

	return 0;

aead_map_failure:
	dx_buffer_mgr_unmap_aead_request(dev, req);
	return rc;
}

int dx_buffer_mgr_map_ahash_request_final(
	struct dx_drvdata *drvdata, struct ahash_request *req, bool do_update)
{
	struct ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = &drvdata->plat_dev->dev;
	uint8_t* curr_buff = areq_ctx->buff_index ? areq_ctx->buff1 :
			areq_ctx->buff0;
	uint32_t *curr_buff_cnt = areq_ctx->buff_index ? &areq_ctx->buff1_cnt :
			&areq_ctx->buff0_cnt;
	struct mlli_params *mlli_params = &areq_ctx->mlli_params;	
	struct buffer_array sg_data;
	struct buff_mgr_handle *buff_mgr = drvdata->buff_mgr_handle;
	int dummy = 0;

	DX_LOG_DEBUG(" final params : curr_buff=%pK "
		     "curr_buff_cnt=0x%X req->nbytes = 0x%X "
		     "req->src=%pK curr_index=%u\n",
		     curr_buff, *curr_buff_cnt, req->nbytes,
		     req->src, areq_ctx->buff_index);
	/* Init the type of the dma buffer */
	areq_ctx->data_dma_buf_type = DX_DMA_BUF_NULL;
	mlli_params->curr_pool = NULL;
	sg_data.num_of_buffers = 0;
	areq_ctx->in_nents = 0;

	if (unlikely(req->nbytes == 0 && *curr_buff_cnt == 0)) {
		/* nothing to do */
		return 0;
	}
	
	/*TODO: copy data in case that buffer is enough for operation */
	/* map the previous buffer */
	if (*curr_buff_cnt != 0 ) {
		if (dx_ahash_handle_curr_buf(dev, areq_ctx, curr_buff,
					    *curr_buff_cnt, &sg_data) != 0) {
			return -ENOMEM;
		}
	}

	if ((req->nbytes > 0) && do_update) {
		if ( unlikely( dx_buffer_mgr_map_scatterlist( dev,req->src,
					  req->nbytes,
					  DMA_TO_DEVICE,
					  &areq_ctx->in_nents,
					  LLI_MAX_NUM_OF_DATA_ENTRIES,
					  &dummy))){
			goto unmap_curr_buff;
		}
		if ( (areq_ctx->in_nents == 1) 
		     && (areq_ctx->data_dma_buf_type == DX_DMA_BUF_NULL) ) {
			memcpy(areq_ctx->buff_sg,req->src,
			       sizeof(struct scatterlist));
			areq_ctx->buff_sg->length = req->nbytes;
			areq_ctx->curr_sg = areq_ctx->buff_sg;
			areq_ctx->data_dma_buf_type = DX_DMA_BUF_DLLI;
		} else {
			areq_ctx->data_dma_buf_type = DX_DMA_BUF_MLLI;
		}

	}

	/*build mlli */
	if (unlikely(areq_ctx->data_dma_buf_type == DX_DMA_BUF_MLLI)) {
		mlli_params->curr_pool = buff_mgr->mlli_buffs_pool;
		/* add the src data to the sg_data */
		dx_buffer_mgr_add_scatterlist_entry(&sg_data,
					areq_ctx->in_nents,
					req->src,
					req->nbytes,
					true);
		if (unlikely(dx_buffer_mgr_generate_mlli(dev, &sg_data,
						  mlli_params) != 0)) {
			goto fail_unmap_din;
		}
	}
	/* change the buffer index for the unmap function */
	areq_ctx->buff_index = (areq_ctx->buff_index^1);
	DX_LOG_DEBUG("areq_ctx->data_dma_buf_type = %s\n",
		GET_DMA_BUFFER_TYPE(areq_ctx->data_dma_buf_type));
	return 0;

fail_unmap_din:
	dma_unmap_sg(dev, req->src, areq_ctx->in_nents, DMA_TO_DEVICE);

unmap_curr_buff:
	if (*curr_buff_cnt != 0 ) {
		dma_unmap_sg(dev, areq_ctx->buff_sg, 1, DMA_TO_DEVICE);
	}
	return -ENOMEM;
}


int dx_buffer_mgr_map_ahash_request_update(
	struct dx_drvdata *drvdata, struct ahash_request *req)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	struct device *dev = &drvdata->plat_dev->dev;
	uint8_t* curr_buff = areq_ctx->buff_index ? areq_ctx->buff1 :
			areq_ctx->buff0;
	uint32_t *curr_buff_cnt = areq_ctx->buff_index ? &areq_ctx->buff1_cnt :
			&areq_ctx->buff0_cnt;
	uint8_t* next_buff = areq_ctx->buff_index ? areq_ctx->buff0 :
			areq_ctx->buff1;
	uint32_t *next_buff_cnt = areq_ctx->buff_index ? &areq_ctx->buff0_cnt :
			&areq_ctx->buff1_cnt;
	unsigned int block_size = crypto_tfm_alg_blocksize(&ahash->base);
	struct mlli_params *mlli_params = &areq_ctx->mlli_params;	
	unsigned int update_data_len;
	int total_in_len = req->nbytes + *curr_buff_cnt;
	struct buffer_array sg_data;
	struct buff_mgr_handle *buff_mgr = drvdata->buff_mgr_handle;
	unsigned int swap_index = 0;
	int dummy = 0;
		
	DX_LOG_DEBUG(" update params : curr_buff=%pK "
		     "curr_buff_cnt=0x%X req->nbytes=0x%X "
		     "req->src=%pK curr_index=%u \n",
		     curr_buff, *curr_buff_cnt, req->nbytes,
		     req->src, areq_ctx->buff_index);
	/* Init the type of the dma buffer */
	areq_ctx->data_dma_buf_type = DX_DMA_BUF_NULL;
	mlli_params->curr_pool = NULL;
	areq_ctx->curr_sg = NULL;
	sg_data.num_of_buffers = 0;
	areq_ctx->in_nents = 0;

	if (unlikely(total_in_len < block_size)) {
		DX_LOG_DEBUG(" less than one block: curr_buff=%pK "
			     "*curr_buff_cnt=0x%X copy_to=%pK\n",
			curr_buff, *curr_buff_cnt,
			&curr_buff[*curr_buff_cnt]);
		areq_ctx->in_nents = 
			dx_buffer_mgr_get_sgl_nents(req->src,
						    req->nbytes,
						    &dummy, NULL);
		sg_copy_to_buffer(req->src, areq_ctx->in_nents,
				  &curr_buff[*curr_buff_cnt], req->nbytes); 
		*curr_buff_cnt += req->nbytes;
		return 1;
	}

	/* Calculate the residue size*/
	*next_buff_cnt = total_in_len & (block_size - 1);
	/* update data len */
	update_data_len = total_in_len - *next_buff_cnt;

	DX_LOG_DEBUG(" temp length : *next_buff_cnt=0x%X "
		     "update_data_len=0x%X\n",
		*next_buff_cnt, update_data_len);

	/* Copy the new residue to next buffer */
	if (*next_buff_cnt != 0) {
		DX_LOG_DEBUG(" handle residue: next buff %pK skip data %u"
			     " residue %u \n", next_buff,
			     (update_data_len - *curr_buff_cnt),
			     *next_buff_cnt);
		dx_buffer_mgr_copy_scatterlist_portion(next_buff, req->src,
			     (update_data_len -*curr_buff_cnt),
			     req->nbytes,DX_SG_TO_BUF);
		/* change the buffer index for next operation */
		swap_index = 1;
	}

	if (*curr_buff_cnt != 0) {
		if (dx_ahash_handle_curr_buf(dev, areq_ctx, curr_buff,
					    *curr_buff_cnt, &sg_data) != 0) {
			return -ENOMEM;
		}
		/* change the buffer index for next operation */
		swap_index = 1;
	}
	
	if ( update_data_len > *curr_buff_cnt ) {
		if ( unlikely( dx_buffer_mgr_map_scatterlist( dev,req->src,
					  (update_data_len -*curr_buff_cnt),
					  DMA_TO_DEVICE,
					  &areq_ctx->in_nents,
					  LLI_MAX_NUM_OF_DATA_ENTRIES,
					  &dummy))){
			goto unmap_curr_buff;
		}
		if ( (areq_ctx->in_nents == 1) 
		     && (areq_ctx->data_dma_buf_type == DX_DMA_BUF_NULL) ) {
			/* only one entry in the SG and no previous data */
			memcpy(areq_ctx->buff_sg,req->src,
			       sizeof(struct scatterlist));
			areq_ctx->buff_sg->length = update_data_len;
			areq_ctx->data_dma_buf_type = DX_DMA_BUF_DLLI;
			areq_ctx->curr_sg = areq_ctx->buff_sg;
		} else {
			areq_ctx->data_dma_buf_type = DX_DMA_BUF_MLLI;
		}
	}

	if (unlikely(areq_ctx->data_dma_buf_type == DX_DMA_BUF_MLLI)) {
		mlli_params->curr_pool = buff_mgr->mlli_buffs_pool;
		/* add the src data to the sg_data */
		dx_buffer_mgr_add_scatterlist_entry(&sg_data,
					areq_ctx->in_nents,
					req->src,
					(update_data_len -*curr_buff_cnt),
					true);
		if (unlikely(dx_buffer_mgr_generate_mlli(dev, &sg_data,
						  mlli_params) != 0)) {
			goto fail_unmap_din;
		}

	}
	areq_ctx->buff_index = (areq_ctx->buff_index^swap_index);

	return 0;

fail_unmap_din:
	dma_unmap_sg(dev, req->src, areq_ctx->in_nents, DMA_TO_DEVICE);

unmap_curr_buff:
	if (*curr_buff_cnt != 0 ) {
		dma_unmap_sg(dev, areq_ctx->buff_sg, 1, DMA_TO_DEVICE);
	}
	return -ENOMEM;
}

void dx_buffer_mgr_unmap_ahash_request(
	struct device *dev, struct ahash_request *req, bool do_revert)
{
	struct ahash_req_ctx *areq_ctx = ahash_request_ctx(req);
	uint32_t *prev_len = areq_ctx->buff_index ?  &areq_ctx->buff0_cnt :
						&areq_ctx->buff1_cnt;

	/*In case a pool was set, a table was 
	  allocated and should be released */
	if (areq_ctx->mlli_params.curr_pool != NULL) {
		DX_LOG_DEBUG("free MLLI buffer: dma=0x%llX virt=%pK\n", 
			     (unsigned long long)areq_ctx->mlli_params.mlli_dma_addr,
			     areq_ctx->mlli_params.mlli_virt_addr);
		dma_pool_free(areq_ctx->mlli_params.curr_pool,
			      areq_ctx->mlli_params.mlli_virt_addr,
			      areq_ctx->mlli_params.mlli_dma_addr);
	}
	
	if (likely(areq_ctx->in_nents != 0)) {
		DX_LOG_DEBUG("Unmapped sg src: virt=%pK dma=0x%llX len=0x%X\n",
			     sg_virt(req->src),
			     (unsigned long long)sg_dma_address(req->src), 
			     sg_dma_len(req->src));

		dma_unmap_sg(dev, req->src, 
			     areq_ctx->in_nents, DMA_TO_DEVICE);
	}

	if (*prev_len != 0) {
		DX_LOG_DEBUG("Unmapped buffer: areq_ctx->buff_sg=%pK"
			     "dma=0x%llX len 0x%X\n", 
				sg_virt(areq_ctx->buff_sg),
				(unsigned long long)sg_dma_address(areq_ctx->buff_sg), 
				sg_dma_len(areq_ctx->buff_sg));
		dma_unmap_sg(dev, areq_ctx->buff_sg, 1, DMA_TO_DEVICE);
		if (!do_revert) {
			/* clean the previous data length for update operation */
			*prev_len = 0;
		} else {
			areq_ctx->buff_index ^= 1;
		}
	}
}

/**
 * dx_buffer_mgr_init_scatterlist_one() - This function should be used by kerenl user
 * to build specail scatter list for the secure key operation, the output of
 * this function is one entry scatter list with the below paramters
 * 
 * #ifdef CONFIG_DEBUG_SG
 *	unsigned long	sg_magic; - set to default in sg_init_table.
 * #endif
 *	unsigned long	page_link; - page link is NULL and the last flag is set.
 *	unsigned int	offset; - offset is zero
 *	unsigned int	length; - buffer length according to the user param.
 *	dma_addr_t	dma_address; - dma_addr accroding to user param.
 * #ifdef CONFIG_NEED_SG_DMA_LENGTH
 *	unsigned int	dma_length; - buffer length according to the user param.
 * #endif
 *
 * @sgl:
 * @addr:
 * @length:
 */
int dx_buffer_mgr_init_scatterlist_one(
	struct scatterlist *sgl, dma_addr_t addr, unsigned int length)
{
	if (sgl == NULL) {
		return -EINVAL;
	}
	/* Init the table */
	sg_init_table(sgl, 1);
	/* set the dma addr */
	sg_dma_address(sgl) = addr;
	/* set the length */
	sg_dma_len(sgl) = length;

	return 0;
}
EXPORT_SYMBOL(dx_buffer_mgr_init_scatterlist_one);

int dx_buffer_mgr_init(struct dx_drvdata *drvdata)
{
	struct buff_mgr_handle *buff_mgr_handle;
	struct device *dev = &drvdata->plat_dev->dev;

	buff_mgr_handle = (struct buff_mgr_handle *)
		kmalloc(sizeof(struct buff_mgr_handle), GFP_KERNEL);
	if (buff_mgr_handle == NULL)
		return -ENOMEM;

	buff_mgr_handle->mlli_buffs_pool = dma_pool_create(
				"dx_single_mlli_tables", dev,
				(2 * LLI_MAX_NUM_OF_DATA_ENTRIES + 
				LLI_MAX_NUM_OF_ASSOC_DATA_ENTRIES) * 
				SEP_LLI_ENTRY_BYTE_SIZE,
				MLLI_TABLE_MIN_ALIGNMENT, 0);

	if (unlikely(buff_mgr_handle->mlli_buffs_pool == NULL))
		goto error;

	drvdata->buff_mgr_handle = buff_mgr_handle;
	return 0;

error:
	dx_buffer_mgr_fini(drvdata);
	return -ENOMEM;
}

int dx_buffer_mgr_fini(struct dx_drvdata *drvdata)
{
	struct buff_mgr_handle *buff_mgr_handle = drvdata->buff_mgr_handle;

	if (buff_mgr_handle  != NULL) {
		dma_pool_destroy(buff_mgr_handle->mlli_buffs_pool);
		kfree(drvdata->buff_mgr_handle);
		drvdata->buff_mgr_handle = NULL;

	}
	return 0;
}

