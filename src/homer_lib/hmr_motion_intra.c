/*****************************************************************************
 * hmr_motion_intra.c : homerHEVC encoding library
/*****************************************************************************
 * Copyright (C) 2014 homerHEVC project
 *
 * Juan Casal <jcasal@homerhevc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include <math.h>
#include <limits.h>
#include <memory.h>


#include "hmr_private.h"
#include "hmr_common.h"
#include "hmr_profiler.h"

#include "hmr_sse42_functions.h"

#ifdef _TIME_PROFILING_
extern profiler_t intra_luma;
extern profiler_t intra_chroma;
extern profiler_t intra_luma_bucle1;
extern profiler_t intra_luma_bucle1_sad;
extern profiler_t intra_luma_bucle2;
extern profiler_t intra_luma_bucle3;
extern profiler_t intra_luma_generate_prediction;
extern profiler_t intra_luma_predict;
extern profiler_t intra_luma_tr;
extern profiler_t intra_luma_q;
extern profiler_t intra_luma_iq;
extern profiler_t intra_luma_itr;
extern profiler_t intra_luma_recon_ssd;
#endif


uint32_t sad(uint8_t * src, uint32_t src_stride, int16_t * pred, uint32_t pred_stride, int size)
{
	int sad = 0;
	int subblock_x, subblock_y;//, chr=0;

	uint8_t * porig;
	int16_t * ppred;

	porig = src;
	ppred =  pred;

	for(subblock_y=0;subblock_y<size;subblock_y++)
	{
		for(subblock_x=0;subblock_x<size;subblock_x++)
		{
			sad += abs(porig[subblock_x] - ppred[subblock_x]);
		}
		porig+=src_stride;
		ppred+=pred_stride;
	}	
	return sad;
}

uint32_t ssd(uint8_t * src, uint32_t src_stride, int16_t * pred, uint32_t pred_stride, int size)
{
	unsigned int ssd = 0;
	int  aux;
	int subblock_x, subblock_y;


	//src_stride-=size;
	//pred_stride-=size;

	for(subblock_y=0;subblock_y<size;subblock_y++)
	{
		for(subblock_x=0;subblock_x<size;subblock_x++)
		{
			aux = src[subblock_x] - pred[subblock_x];ssd +=aux*aux; 
		}
		src+=src_stride;
		pred+=pred_stride;
	}	

/*	for(subblock_y=0;subblock_y<size;subblock_y++)
	{
		for(subblock_x=0;subblock_x<size;subblock_x+=4)
		{
			aux = *src++ - *pred++;ssd +=aux*aux; 
			aux = *src++ - *pred++;ssd +=aux*aux; 
			aux = *src++ - *pred++;ssd +=aux*aux;
			aux = *src++ - *pred++;ssd +=aux*aux; 
		}
		src+=src_stride;
		pred+=pred_stride;
	}	
*/
	return ssd;
}

uint32_t sad16b(int16_t *src, uint32_t src_stride, int16_t *pred, uint32_t pred_stride, int size)
{
	unsigned int sad = 0;
	int x, y;

	for(y=0;y<size;y++)
	{
		for(x=0;x<size;x++)
		{
			sad += abs(src[x] - pred[x]);
		}
		src+=src_stride;
		pred+=pred_stride;
	}

	return sad;
}

uint32_t ssd16b(int16_t *src, uint32_t src_stride, int16_t *pred, uint32_t pred_stride, int size)
{
	unsigned int ssd = 0;
	int  aux;
	int x, y;

	for(y=0;y<size;y++)
	{
		for(x=0;x<size;x++)
		{
			aux = src[x] - pred[x];ssd +=aux*aux; 
		}
		src+=src_stride;
		pred+=pred_stride;
	}

	return ssd;
}


static uint8_t intra_filter[5] =
{
  10, //4x4
  7, //8x8
  1, //16x16
  0, //32x32
  10, //64x64
};	


void predict(uint8_t * orig_auxptr, int orig_buff_stride, int16_t *pred_auxptr, int pred_buff_stride, int16_t * residual_auxptr, int residual_buff_stride, int curr_part_size)
{
	int i,j;
	for(j=0;j<curr_part_size;j++)
	{
		for(i=0;i<curr_part_size;i++)
		{
			residual_auxptr[i] = 	(int16_t)orig_auxptr[i]-pred_auxptr[i];
		}
		residual_auxptr += residual_buff_stride;
		orig_auxptr += orig_buff_stride;
		pred_auxptr += pred_buff_stride;
	}
}

void reconst(int16_t *pred_auxptr, int pred_buff_stride, int16_t * residual_auxptr, int residual_buff_stride, int16_t * decoded_auxptr, int decoded_buff_stride, int curr_part_size)
{
	int i,j;
	for(j=0;j<curr_part_size;j++)
	{
		for(i=0;i<curr_part_size;i++)
		{
			decoded_auxptr[i] = clip(residual_auxptr[i]+pred_auxptr[i],0,255);
		}
		decoded_auxptr += decoded_buff_stride;
		residual_auxptr += residual_buff_stride;//este es 2D.Podria ser lineal
		pred_auxptr += pred_buff_stride;
	}
}


void adi_filter(int16_t  *ptr, int16_t  *ptr_filter, int depth, int adi_size, int partition_size, int max_cu_size_shift, int intra_smooth_enable, int bit_depth)
{
	int i;
	if(intra_smooth_enable)
	{
		int left_bottom = ptr[0];
		int left_top = ptr[2*partition_size];
		int top_right = ptr[adi_size-1];
		int threshold = 1 << (bit_depth - 5);
		int bilinear_left = abs(left_bottom+left_top-2*ptr[partition_size]) < threshold;
		int bilinear_above  = abs(left_top+top_right-2*ptr[2*partition_size+partition_size]) < threshold;

		if (partition_size>=32 && (bilinear_left && bilinear_above))
		{
			int size_shift = max_cu_size_shift-depth +1;
			ptr_filter[0] = ptr[0];
			ptr_filter[2*partition_size] = ptr[2*partition_size];
			ptr_filter[adi_size - 1] = ptr[adi_size - 1];
			for (i = 1; i < 2*partition_size; i++)//left
			{
				ptr_filter[i] = ((2*partition_size-i)*left_bottom + i*left_top + partition_size) >> size_shift;
			}

			for (i = 1; i < 2*partition_size; i++)//tops
			{
				ptr_filter[2*partition_size + i] = ((2*partition_size-i)*left_top + i*top_right + partition_size) >> size_shift;
			}
		}
		else 
		{
			//filter [1,2,1]
			int aux2, aux = ptr_filter[0] = ptr[0];

			for(i=1;i<adi_size-1;i++)	//column + square + row
			{
				aux2 = ptr[i];
				ptr_filter[i] = (aux + 2 * ptr[i]+ptr[i + 1] + 2) >> 2;
				aux = aux2;
			}
			ptr_filter[i] = ptr[i];
		}		
	}
	else
	{
		//filter [1,2,1]
		int aux2, aux = ptr_filter[0] = ptr[0];

		for(i=1;i<adi_size-1;i++)	//column + square + row
		{
			aux2 = ptr[i];
			ptr_filter[i] = (aux + 2 * ptr[i]+ptr[i + 1] + 2) >> 2;
			aux = aux2;
		}
		ptr_filter[i] = ptr[i];
	}
}

void fill_reference_samples(henc_thread_t* et, ctu_info_t* ctu, cu_partition_info_t* partition_info, int adi_size, int16_t* decoded_buff, int decoded_buff_stride, int partition_size, int comp, int is_filtered)
{
	int i;
	int  dc = 1 << (et->bit_depth - 1);//dc value depending on bit-depth
	int16_t  *ptr = et->adi_pred_buff;
	int16_t  *ref_ptr;
	int16_t first_sample,last_sample;
	int16_t  *ptr_padding_left, *ptr_padding_top;
	int padding_left_size = 0, padding_top_size = 0;
	

	ctu->top = 1;
	ctu->left = 1;
	if(partition_info->left_neighbour == 0 && partition_info->top_neighbour == 0)
	{
		for(i=0;i<adi_size;i++)	
			ptr[i] = dc;
	}
	else
	{
		ref_ptr = decoded_buff+(partition_size)*decoded_buff_stride;
		ptr = et->adi_pred_buff+partition_size;

		if(partition_info->left_neighbour)
		{
			for(i=0;i<partition_size;i++)
			{
				*ptr++ = ref_ptr[(-i)*decoded_buff_stride];
			}
			first_sample = ptr[-partition_size];
			last_sample = ptr[-1];
		}
		else
		{
			ptr_padding_left = ptr;
			padding_left_size = partition_size;
		}

//		ref_ptr = decoded_buff+(2*partition_size)*decoded_buff_stride;
//		ptr = et->adi_pred_buff;
		ref_ptr = decoded_buff+(partition_size+1)*decoded_buff_stride;
		ptr = et->adi_pred_buff+partition_size-1;
		if(partition_info->left_bottom_neighbour)
		{
			int left_bottom_size = min(partition_size, et->pict_height[comp]-(ctu->y[comp]+(comp==Y_COMP?partition_info->y_position:partition_info->y_position_chroma)+partition_size));

			//int left_bottom_size = partition_size;//min(partition_size, et->pict_width[comp]-(ctu->x[comp]+2*partition_size));
			for(i=0;i<left_bottom_size;i++)
			{
				*ptr-- = ref_ptr[(i)*decoded_buff_stride];
			}
			first_sample = ptr[1];//ptr[-left_bottom_size];
			if(left_bottom_size!=partition_size)//pic width is not multiple of ctu. we have to pad 
			{
				ptr_padding_left = et->adi_pred_buff;//ptr;
				padding_left_size = partition_size-left_bottom_size;			
			}
		}
		else
		{
			ptr_padding_left = et->adi_pred_buff;
			if(partition_info->left_neighbour)
			{
				padding_left_size = partition_size;
			}
			else
			{
				padding_left_size += partition_size;
			}
		}


		//top
		ptr = ADI_POINTER_MIDDLE(et->adi_pred_buff, adi_size) + 1;
		ref_ptr = decoded_buff+1;

		if(partition_info->top_neighbour)
		{
			for(i=0;i<partition_size;i++)
			{
				*ptr++ = (int16_t)*ref_ptr++;
			}
			if(!partition_info->left_neighbour)
				first_sample = ptr[-partition_size];
			last_sample = ptr[-1];
		}
		else
		{
			ptr_padding_top = ptr;
			padding_top_size = partition_size;		
		}

		if(partition_info->top_right_neighbour)
		{
			int top_right_size = min(partition_size, et->pict_width[comp]-(ctu->x[comp]+(comp==Y_COMP?partition_info->x_position:partition_info->x_position_chroma)+partition_size));

			for(i=0;i<top_right_size;i++)
			{
				*ptr++ = (int16_t)*ref_ptr++;
			}
			last_sample = ptr[-1];
			if(top_right_size!=partition_size)//pic height is not multiple of ctu. we have to pad
			{
				ptr_padding_top = ptr;
				padding_top_size = partition_size-top_right_size;			
			}
		}
		else
		{
			if(partition_info->top_neighbour)
			{
				ptr_padding_top = ptr;
				padding_top_size = partition_size;
			}
			else
				padding_top_size += partition_size;
		}

		ptr = ADI_POINTER_MIDDLE(et->adi_pred_buff, adi_size);
		ref_ptr = decoded_buff;
		if(partition_info->left_neighbour && partition_info->top_neighbour)//square pixel
		{
			*ptr = *ref_ptr;
		}
		else
		{
			if(partition_info->left_neighbour)
			{
				ptr_padding_top--;
				padding_top_size++;
			}
			else//if(partition_info->top_neighbour)
			{
				padding_left_size++;
			}
		}

		//padding left
		for(i=0;i<padding_left_size;i++)
		{
			*ptr_padding_left++ = first_sample;
		}

		//padding top
		for(i=0;i<padding_top_size;i++)
		{
			*ptr_padding_top++ = last_sample;
		}
	}

	if(is_filtered)
	{
		int16_t  *ptr_filter = et->adi_filtered_pred_buff;
		ptr = et->adi_pred_buff;

		adi_filter(ptr, ptr_filter, partition_info->depth, adi_size, partition_size, et->max_cu_size_shift, et->sps->strong_intra_smooth_enabled_flag, et->bit_depth);
	}

}



void create_intra_planar_prediction(henc_thread_t* et, int16_t* pred, int pred_stride, int16_t  *adi_pred_buff, int adi_size, int cu_size, int cu_size_shift)
{
	int i, j, left_bottom, top_right, left, top, horPred;
	int16_t  *adi_ptr = ADI_POINTER_MIDDLE(adi_pred_buff, adi_size);
	int16_t *leftColumn = et->left_pred_buff;
	int16_t *topRow = et->top_pred_buff;
	int16_t *bottomRow = et->bottom_pred_buff;
	int16_t *rightColumn = et->right_pred_buff;

	left_bottom = adi_ptr[-(cu_size+1)];//bottom-left corner
	top_right   = adi_ptr[cu_size+1];//top-right conrner
	for(i=0;i<cu_size;i++)
	{
		left = adi_ptr[-(i+1)];
		top = adi_ptr[i+1];
		bottomRow[i]   = left_bottom - top;
		rightColumn[i] = top_right - left;
		topRow[i]  = top<<cu_size_shift;
		leftColumn[i] = left<<cu_size_shift;
	}

	for (j=0;j<cu_size;j++)
	{
		horPred = leftColumn[j] + cu_size;
		for (i=0;i<cu_size;i++)
		{
		  horPred += rightColumn[j];
		  topRow[i] += bottomRow[i];
		  pred[j*pred_stride+i] = ( (horPred + topRow[i]) >> (cu_size_shift+1) );
		}
	}
}


ushort pred_intra_calc_dc( int16_t *adi_ptr, int width, int height, int top, int left )
{
	int i, aux = 0;
	ushort dc;

	if (top)
	{
		for (i=1;i <= width;i++)
		{
			aux += adi_ptr[i];//top
		}
	}
	if (left)
	{
		for (i=1;i <= height;i++)
		{
			aux += adi_ptr[-(i)];//left
		}
	}

	if (top && left)
	{
		dc = (aux + width) / (width + height);
	}
	else if (top)
	{
		dc = (aux + width/2) / width;
	}
	else if (left)
	{
		dc = (aux + height/2) / height;
	}
	else
	{
		dc = adi_ptr[0];//default dc value already calculated and placed in the prediction array if no neighbors are available
	}

	return dc;
}

void create_intra_angular_prediction(henc_thread_t* et, ctu_info_t* ctu, int16_t *pred, int pred_stride, int16_t  *adi_pred_buff, int adi_size, int cu_size, int cu_mode, int is_luma)//creamos el array de prediccion angular
{
	int i, j;
	int is_DC_mode = cu_mode < 2;
	int is_Hor_mode = !is_DC_mode && (cu_mode < 18);
	int is_Ver_mode = !is_DC_mode && !is_Hor_mode;
	int pred_angle = is_Ver_mode ? cu_mode - VER_IDX : is_Hor_mode ? -(cu_mode - HOR_IDX) : 0;
	int abs_angle = abs(pred_angle);
	int sign_angle = SIGN(pred_angle);
	int16_t  *adi_ptr = ADI_POINTER_MIDDLE(adi_pred_buff, adi_size);
	int inv_angle = et->enc_engine->inv_ang_table[abs_angle];
	int bit_depth = et->bit_depth;
	abs_angle = et->enc_engine->ang_table[abs_angle];
	pred_angle = sign_angle * abs_angle;

	if (is_DC_mode)
	{
		ushort dcval = pred_intra_calc_dc(adi_ptr, cu_size, cu_size, ctu->top, ctu->left);

		for (j=0;j<cu_size;j++)
		{
			for (i=0;i<cu_size;i++)
			{
				pred[j*pred_stride+i] = (uint8_t)dcval;
			}
		}

		if(cu_size<=16 && cu_mode == DC_IDX && is_luma)
		{
			pred[0] = ((adi_ptr[-1] + adi_ptr[1] + 2*(int16_t)pred[0] + 2)>>2);

			adi_ptr = ADI_POINTER_MIDDLE(adi_pred_buff, adi_size) + 1;
			for(i=1;i<cu_size;i++)		
			{
				pred[i] = ((adi_ptr[i] + 3*((int16_t)pred[i]) + 2)>>2);
			}
			adi_ptr = ADI_POINTER_MIDDLE(adi_pred_buff, adi_size) - 1;
			for(j=1;j<cu_size;j++)		
			{
				pred[j*pred_stride] = ((adi_ptr[-j] + 3*(int16_t)pred[j*pred_stride] + 2)>>2);
			}
		}
	}
	else//angular predictions
	{
		int16_t* refMain;
		int16_t* refSide;
		int16_t  *refAbove = et->top_pred_buff;
		int16_t  *refLeft = et->left_pred_buff;
		int invAngleSum;
		int bFilter = is_luma?(cu_size<=16):0;

		if (pred_angle < 0)
		{
			{
				for (i=0;i<cu_size+1;i++)
				{
					refAbove[i+cu_size-1] = adi_ptr[i];
				}
				for (i=0;i<cu_size+1;i++)
				{
					refLeft[i+cu_size-1] = adi_ptr[-(i)];
				}
			}
			refMain = (is_Ver_mode ? refAbove : refLeft) + (cu_size-1);
			refSide = (is_Ver_mode ? refLeft : refAbove) + (cu_size-1);

			invAngleSum    = 128;       
			for (i=-1; i>cu_size*pred_angle>>5; i--)
			{
				invAngleSum += inv_angle;
				refMain[i] = refSide[invAngleSum>>8];
			}
		}
		else	
		{
			{
				for (i=0;i<2*cu_size+1;i++)
				{
					refAbove[i] = adi_ptr[i];
				}
				for (i=0;i<2*cu_size+1;i++)
				{
					refLeft[i] = adi_ptr[-(i)];
				}
			}
			refMain = is_Ver_mode ? refAbove : refLeft;
			refSide = is_Ver_mode ? refLeft  : refAbove;
		}

		if (pred_angle == 0)
		{
			int aux_stride = is_Hor_mode?1:pred_stride;
			int aux_stride2 = is_Hor_mode?pred_stride:1;
			for (j=0;j<cu_size;j++)
			{
				for (i=0;i<cu_size;i++)
				{
					pred[j*aux_stride+i*aux_stride2] = (uint8_t)refMain[i+1];
				}
			}

			if ( bFilter )
			{
				for (i=0;i<cu_size;i++)
				{
					pred[i*aux_stride] = clip(pred[i*aux_stride] + (( refSide[i+1] - refSide[0] ) >> 1) , 0, (1<<bit_depth)-1);
				}
			}
		}
		else
		{
			int pos_delta=0;
			int aux_delta;
			int fract_delta;
			int ref_main_idx;
			int aux_stride = is_Hor_mode?1:pred_stride;//if is_Hor_mode le switch order (exchange x and y strides)
			int aux_stride2 = is_Hor_mode?pred_stride:1;
			for (j=0;j<cu_size;j++)
			{
				pos_delta += pred_angle;
				aux_delta   = pos_delta >> 5;
				fract_delta = pos_delta & (32 - 1);

				if (fract_delta)
				{
					// linear filter
					for (i=0;i<cu_size;i++)
					{
						ref_main_idx        = i+aux_delta+1;
						pred[j*aux_stride+i*aux_stride2] = (uint8_t) ( ((32-fract_delta)*refMain[ref_main_idx]+fract_delta*refMain[ref_main_idx+1]+16) >> 5 );
					}
				}
				else
				{
					for (i=0;i<cu_size;i++)
					{
						pred[j*aux_stride+i*aux_stride2] = (uint8_t)refMain[i+aux_delta+1];
					}
				}
			}
		}
	}
}

//can not be called for CTU
//void cu_partition_get_neighbours(cu_partition_info_t *curr_part, int cu_size)
void cu_partition_get_neighbours(cu_partition_info_t *curr_part, int cu_size, int ctu_valid_colums, int ctu_valid_lines)
{
	cu_partition_info_t	*parent = curr_part->parent;

	if(parent->left_neighbour || curr_part->x_position)
		curr_part->left_neighbour = 1;
	else
		curr_part->left_neighbour = 0;

	if(parent->top_neighbour || curr_part->y_position)
		curr_part->top_neighbour = 1;
	else
		curr_part->top_neighbour = 0;


	if((parent->left_bottom_neighbour && curr_part->x_position == parent->x_position) ||
	  (parent->left_neighbour && curr_part->x_position == parent->x_position && curr_part->y_position == parent->y_position && ctu_valid_lines>curr_part->y_position+curr_part->size))// && parent->is_b_inside_frame))
		curr_part->left_bottom_neighbour = 1;
	else 
		curr_part->left_bottom_neighbour = 0;

	if((parent->top_right_neighbour && curr_part->y_position == parent->y_position) ||
	  (parent->top_neighbour && curr_part->x_position == parent->x_position && curr_part->y_position == parent->y_position && ctu_valid_colums>curr_part->x_position+curr_part->size) ||
	  (curr_part->x_position == parent->x_position && curr_part->y_position != parent->y_position && ctu_valid_colums>curr_part->x_position+curr_part->size))//parent->is_r_inside_frame))
		curr_part->top_right_neighbour = 1;
	else 
		curr_part->top_right_neighbour = 0;
}

void create_partition_ctu_neighbours(henc_thread_t* et, ctu_info_t *ctu, cu_partition_info_t* curr_partition_info)
{
	cu_partition_info_t*	parent_part_info = curr_partition_info->parent;
	int curr_depth = curr_partition_info->depth;
	int depth_state[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	int cu_min_tu_size_shift = max((et->max_cu_size_shift - (et->max_pred_partition_depth+max(et->max_intra_tr_depth, et->max_inter_tr_depth)-1)), MIN_TU_SIZE_SHIFT);//(et->max_cu_size_shift - (et->max_pred_partition_depth+et->max_intra_tr_depth-1))>MIN_TU_SIZE_SHIFT?(et->max_cu_size_shift - (et->max_pred_partition_depth+et->max_intra_tr_depth)):MIN_TU_SIZE_SHIFT;//+ interSplitFlag + intraSplitFlag	//(part_size_type==SIZE_NxN));//
	int max_processing_depth = et->max_cu_size_shift-cu_min_tu_size_shift;
	int ctu_valid_lines = (ctu->y[Y_COMP]+ctu->size)>et->pict_height[Y_COMP]?(et->pict_height[Y_COMP]-ctu->y[Y_COMP]):ctu->size;//((ctu->y[Y_COMP]+ctu->size)>et->pict_height)?(et->pict_height-ctu->y[Y_COMP]):ctu->size;
	int ctu_valid_colums = (ctu->x[Y_COMP]+ctu->size)>et->pict_width[Y_COMP]?(et->pict_width[Y_COMP]-ctu->x[Y_COMP]):ctu->size;//((ctu->y[Y_COMP]+ctu->size)>et->pict_width)?(et->pict_width-ctu->y[Y_COMP]):ctu->size;


	while(curr_depth!=0 || depth_state[curr_depth]!=1)
	{
		curr_depth = curr_partition_info->depth;
		curr_partition_info->is_tl_inside_frame = (ctu->y[Y_COMP]+curr_partition_info->y_position < et->pict_height[Y_COMP]) && (ctu->x[Y_COMP]+curr_partition_info->x_position < et->pict_width[Y_COMP]);
		curr_partition_info->is_b_inside_frame = (ctu->y[Y_COMP]+curr_partition_info->y_position+curr_partition_info->size <= et->pict_height[Y_COMP]);
		curr_partition_info->is_r_inside_frame = (ctu->x[Y_COMP]+curr_partition_info->x_position+curr_partition_info->size <= et->pict_width[Y_COMP]);

		if(curr_partition_info->is_tl_inside_frame)
		{
			if(parent_part_info == NULL)//ctu
			{
				curr_partition_info->left_neighbour = ctu->ctu_left?1:0;
				curr_partition_info->top_neighbour = ctu->ctu_top?1:0;
				curr_partition_info->left_bottom_neighbour = ctu->ctu_left_bottom?1:0;
				curr_partition_info->top_right_neighbour = ctu->ctu_top_right?1:0; 
			}
			else
				cu_partition_get_neighbours(curr_partition_info, et->max_cu_size, ctu_valid_colums, ctu_valid_lines);
		}

		depth_state[curr_depth]++;
		if(curr_depth<max_processing_depth && curr_partition_info->is_tl_inside_frame)//if tl is not inside the frame don't process the next depths
		{
			curr_depth++;
			parent_part_info = curr_partition_info;
		}
		else if(depth_state[curr_depth]==4)
		{
			while(depth_state[curr_depth]==4)
			{
				depth_state[curr_depth] = 0;
				curr_depth--;
				parent_part_info = parent_part_info->parent;
			}
		}
		if(parent_part_info!=NULL)
			curr_partition_info = parent_part_info->children[depth_state[curr_depth]];
	}
}


void get_partition_neigbours(hvenc_engine_t* enc_engine, cu_partition_info_t* curr_partition_info)
{
	int ctu_partitions_in_line = (enc_engine->max_cu_size)>>(enc_engine->num_partitions_in_cu_shift>>1);
	int ctu_partitions_in_line_mask = ctu_partitions_in_line-1;
	int ctu_partitions_in_ctu_mask = enc_engine->num_partitions_in_cu-1;
	int ctu_partition_top_line_offset = enc_engine->num_partitions_in_cu-ctu_partitions_in_line;
	int left_raster_index, left_bottom_raster_index, top_raster_index, top_right_raster_index, top_left_raster_index;
	int	raster_index = curr_partition_info->raster_index;

	//left
	if((raster_index & ctu_partitions_in_line_mask) == 0)
	{
		left_raster_index = raster_index + ctu_partitions_in_line_mask;
	}
	else
	{
		left_raster_index = raster_index - 1;
	}

	//left bottom
	left_bottom_raster_index = (left_raster_index + ctu_partitions_in_line) & ctu_partitions_in_ctu_mask;

	//top
	top_raster_index = (raster_index + ctu_partition_top_line_offset) & ctu_partitions_in_ctu_mask;

	//top right
	if((top_raster_index & ctu_partitions_in_line_mask) == ctu_partitions_in_line_mask)//is in the right column of the ctu
	{
		top_right_raster_index = top_raster_index - ctu_partitions_in_line_mask;
	}
	else
	{
		top_right_raster_index = top_raster_index+1;
	}

	//top left
	top_left_raster_index = (left_raster_index + ctu_partition_top_line_offset) & ctu_partitions_in_ctu_mask;

	curr_partition_info->abs_index_left_partition = enc_engine->raster2abs_table[left_raster_index];
	curr_partition_info->abs_index_left_bottom_partition = enc_engine->raster2abs_table[left_bottom_raster_index];
	curr_partition_info->abs_index_top_partition = enc_engine->raster2abs_table[top_raster_index];
	curr_partition_info->abs_index_top_right_partition = enc_engine->raster2abs_table[top_right_raster_index];
	curr_partition_info->abs_index_top_left_partition = enc_engine->raster2abs_table[top_left_raster_index];
}



//creates tree depencies
void init_partition_info(hvenc_engine_t* enc_engine, cu_partition_info_t *partition_list)
{
	int curr_depth = 0;
	int next_depth_change = 0;
	int num_qtree_partitions;
	int curr_part_x, curr_part_y;
	int curr_part_size = enc_engine->max_cu_size, parent_size;
	int num_part_in_cu;
	int curr_cu_part;
	int parent_cnt, child_cnt;
	cu_partition_info_t*	curr_partition_info;
	cu_partition_info_t*	parent_part_info = partition_list;

	parent_part_info->cost = INT_MAX;// = parent_part_info->cost_chroma = INT_MAX;
	parent_part_info = partition_list;
	parent_part_info->list_index = 0;
	parent_part_info->size = curr_part_size;
	parent_part_info->size_chroma = curr_part_size>>1;
	parent_part_info->depth = curr_depth;
	parent_part_info->abs_index = 0;
	parent_part_info->num_part_in_cu = ((curr_part_size*curr_part_size)>>enc_engine->num_partitions_in_cu_shift);
	parent_part_info->parent = NULL;//pointer to parent partition
	get_partition_neigbours(enc_engine, parent_part_info);

	num_qtree_partitions = 0;
	//calculate the number of iterations needed to cover all the tree elements (nodes and leaves)


	for(curr_depth=0;curr_depth<MAX_PARTITION_DEPTH-1;curr_depth++)	//we don't iterate on leaves, just nodes. Thats why < is used
		num_qtree_partitions += (1<<(2*(curr_depth)));

	curr_depth = 0;
	next_depth_change = 0;//we consider the parent is already processed
	curr_cu_part = 1;//we consider the is parent already processed

	for(parent_cnt=0;parent_cnt<num_qtree_partitions;parent_cnt++)
	{
		//all this vars define the state of the iteration 	
		parent_part_info = &partition_list[parent_cnt];

		if(parent_cnt == next_depth_change)
		{
			curr_depth++;
			next_depth_change += (1<<(2*(curr_depth-1)));
			enc_engine->partition_depth_start[curr_depth] = next_depth_change;
			parent_size = curr_part_size;
			curr_part_size = enc_engine->max_cu_size>>curr_depth;
//			curr_part_size_shift = et->max_cu_size_shift-curr_depth;
			num_part_in_cu =  ((curr_part_size*curr_part_size)>>enc_engine->num_partitions_in_cu_shift);
		}

		curr_part_x = 0;//these are partial coordiatens inside the CU
		curr_part_y = 0;
		//set partition info
		for(child_cnt=0;child_cnt<4;child_cnt++,curr_cu_part++) 
		{
			curr_partition_info = &partition_list[curr_cu_part];//0 is the CTU info
			parent_part_info->children[child_cnt] = curr_partition_info;
			curr_partition_info->parent = parent_part_info;

			curr_partition_info->list_index = curr_cu_part;
			curr_partition_info->x_position = parent_part_info->x_position+curr_part_x;//relative to cu
			curr_partition_info->y_position = parent_part_info->y_position+curr_part_y;//relative to cu
			curr_partition_info->x_position_chroma = curr_partition_info->x_position>>1;//relative to cu
			curr_partition_info->y_position_chroma = curr_partition_info->y_position>>1;//relative to cu
			curr_partition_info->size = curr_part_size;
			curr_partition_info->size_chroma = curr_part_size>>1;
			curr_partition_info->depth = curr_depth;
			curr_partition_info->abs_index = parent_part_info->abs_index+child_cnt*num_part_in_cu;
			curr_partition_info->raster_index = enc_engine->abs2raster_table[curr_partition_info->abs_index];
			get_partition_neigbours(enc_engine, curr_partition_info);


			curr_partition_info->num_part_in_cu = num_part_in_cu;
			
			curr_part_x += curr_part_size;
			if(curr_part_x >= parent_size)//next row
			{
				curr_part_x = 0;
				curr_part_y += curr_part_size;
			}
		}	
	}
}


void synchronize_reference_buffs(henc_thread_t* et, cu_partition_info_t* curr_part, wnd_t *decoded_src, wnd_t * decoded_dst, int gcnt)
{
	int j;//, i;
	int decoded_buff_stride = WND_STRIDE_2D(*decoded_src, Y_COMP);//-curr_part->size;
	int16_t * decoded_buff_src = WND_POSITION_2D(int16_t *, *decoded_src, Y_COMP, curr_part->x_position, curr_part->y_position, gcnt, et->ctu_width);
	int16_t * decoded_buff_dst = WND_POSITION_2D(int16_t *, *decoded_dst, Y_COMP, curr_part->x_position, curr_part->y_position, gcnt, et->ctu_width);

	//bottom line
	memcpy(decoded_buff_dst+decoded_buff_stride*(curr_part->size-1), decoded_buff_src+decoded_buff_stride*(curr_part->size-1), curr_part->size*sizeof(decoded_buff_src[0]));

	//right column
	decoded_buff_src+=curr_part->size-1;
	decoded_buff_dst+=curr_part->size-1;
	for(j=0;j<curr_part->size-1;j++)
	{
		decoded_buff_dst[j*decoded_buff_stride] = decoded_buff_src[j*decoded_buff_stride];
	}	
}



//this function is used to consolidate buffers from bottom to top
void synchronize_motion_buffers_luma(henc_thread_t* et, cu_partition_info_t* curr_part, wnd_t * quant_src, wnd_t * quant_dst, wnd_t *decoded_src, wnd_t * decoded_dst, int gcnt)
{
	int j;//, i;
	int decoded_buff_stride = WND_STRIDE_2D(*decoded_src, Y_COMP);//-curr_part->size;
	int16_t * decoded_buff_src = WND_POSITION_2D(int16_t *, *decoded_src, Y_COMP, curr_part->x_position, curr_part->y_position, gcnt, et->ctu_width);
	int16_t * decoded_buff_dst = WND_POSITION_2D(int16_t *, *decoded_dst, Y_COMP, curr_part->x_position, curr_part->y_position, gcnt, et->ctu_width);

	int quant_buff_stride = curr_part->size;//0;//es lineal
	int16_t * quant_buff_src = WND_POSITION_1D(int16_t  *, *quant_src, Y_COMP, gcnt, et->ctu_width, (curr_part->abs_index<<et->num_partitions_in_cu_shift));
	int16_t * quant_buff_dst = WND_POSITION_1D(int16_t  *, *quant_dst, Y_COMP, gcnt, et->ctu_width, (curr_part->abs_index<<et->num_partitions_in_cu_shift));

	for(j=0;j<curr_part->size;j++)
	{
		memcpy(quant_buff_dst, quant_buff_src, curr_part->size*sizeof(quant_buff_src[0]));
		memcpy(decoded_buff_dst, decoded_buff_src, curr_part->size*sizeof(decoded_buff_src[0]));
		quant_buff_src += quant_buff_stride;
		quant_buff_dst += quant_buff_stride;
		decoded_buff_src += decoded_buff_stride;
		decoded_buff_dst += decoded_buff_stride;
	}
}


void homer_update_cand_list( uint uiMode, double Cost, uint BitCost, int CandModeList[3], double CandCostList[3], uint BitCostList[3])
{
	uint i;
	uint aux_mode;	
	double aux_cost;
	uint aux_bitcost;
	{
		for(i=0;i<3;i++)
		{
			if(CandCostList[i]>Cost)
			{
				aux_mode = CandModeList[i] ;
				aux_cost = CandCostList[i];
				aux_bitcost = BitCostList[i];
				CandCostList[i] = Cost;
				CandModeList[i] = uiMode;
				BitCostList[i] = BitCost;
				Cost = aux_cost;
				uiMode=aux_mode;
				BitCost=aux_bitcost;
			}
		}
	}
}

void hm_update_cand_list( uint uiMode, double Cost, uint uiFastCandNum, int CandModeList[3], double CandCostList[3] )
{
	uint i;
	uint shift=0;

	while ( shift<uiFastCandNum && Cost<CandCostList[ uiFastCandNum-1-shift ] ) shift++;

	if( shift!=0 )
	{
		for(i=1; i<shift; i++)
		{
			CandModeList[ uiFastCandNum-i ] = CandModeList[ uiFastCandNum-1-i ];
			CandCostList[ uiFastCandNum-i ] = CandCostList[ uiFastCandNum-1-i ];
		}
		CandModeList[ uiFastCandNum-shift ] = uiMode;
		CandCostList[ uiFastCandNum-shift ] = Cost;
	}
}

uint32_t modified_variance(uint8_t *p, int size, int stride, int modif)
{
	int i,j, v;
	unsigned int s,s2;
	uint8_t *paux = p;
	s = s2 = 0;

	//avg
	for (j=0; j<size; j++)
	{
		for (i=0; i<size; i++)
		{
			v = *paux++;
			s+= v;	
		}
		paux+= stride-size;
	}
	s/=(size*size);

	paux = p;
	for (j=0; j<size; j++)
	{
		for (i=0; i<size; i++)
		{
			v = (*paux++ - s);
			v=1 + (v)*modif;
			s2+= v*v;
		}
		paux+= stride-size;
	}

	return s2;
}



uint encode_intra_cu(henc_thread_t* et, ctu_info_t* ctu, cu_partition_info_t* curr_partition_info, int depth, int cu_mode, PartSize part_size_type, int *curr_sum, int gcnt)//depth = prediction depth
{		
	int ssd_;
	int pred_buff_stride, orig_buff_stride, residual_buff_stride, decoded_buff_stride;
	uint8_t *orig_buff;
	int16_t *pred_buff, *residual_buff, *quant_buff, *iquant_buff, *decoded_buff;
	uint8_t *cbf_buff = NULL;
	wnd_t *quant_wnd = NULL, *decoded_wnd = NULL;
	int inv_depth, diff, is_filtered;
		
	int curr_depth = curr_partition_info->depth;
	int curr_part_x = curr_partition_info->x_position;
	int curr_part_y = curr_partition_info->y_position;
	int curr_part_size = curr_partition_info->size;
	int curr_part_size_shift = et->max_cu_size_shift-curr_depth;
	int curr_adi_size = 2*2*curr_part_size+1;

	int curr_scan_mode = find_scan_mode(TRUE, TRUE, curr_part_size, cu_mode, 0);
	int per = curr_partition_info->qp/6;
	int rem = curr_partition_info->qp%6; 

	quant_wnd = et->transform_quant_wnd[curr_depth+1];
	decoded_wnd = et->decoded_mbs_wnd[curr_depth+1];
//	cbf_buff = et->cbf_buffs[Y_COMP][curr_depth];

	pred_buff_stride = WND_STRIDE_2D(et->prediction_wnd, Y_COMP);
	pred_buff = WND_POSITION_2D(int16_t *, et->prediction_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);
	orig_buff_stride = WND_STRIDE_2D(et->curr_mbs_wnd, Y_COMP);
	orig_buff = WND_POSITION_2D(uint8_t *, et->curr_mbs_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);
	residual_buff_stride = WND_STRIDE_2D(et->residual_wnd, Y_COMP);
	residual_buff = WND_POSITION_2D(int16_t *, et->residual_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);
	quant_buff = WND_POSITION_1D(int16_t  *, *quant_wnd, Y_COMP, gcnt, et->ctu_width, (curr_partition_info->abs_index<<et->num_partitions_in_cu_shift));
	iquant_buff = WND_POSITION_1D(int16_t  *, et->itransform_iquant_wnd, Y_COMP, gcnt, et->ctu_width, (curr_partition_info->abs_index<<et->num_partitions_in_cu_shift));
	decoded_buff_stride = WND_STRIDE_2D(*decoded_wnd, Y_COMP);
	decoded_buff = WND_POSITION_2D(int16_t *, *decoded_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);
	quant_buff = WND_POSITION_1D(int16_t  *, *quant_wnd, Y_COMP, gcnt, et->ctu_width, (curr_partition_info->abs_index<<et->num_partitions_in_cu_shift));

	inv_depth = (et->max_cu_size_shift - curr_depth);
	diff = min(abs(cu_mode - HOR_IDX), abs(cu_mode - VER_IDX));
	is_filtered = (cu_mode!=DC_IDX) && ((diff > intra_filter[inv_depth - 2]) ? 1 : 0);//is_luma?((diff > intra_filter[inv_depth - 2]) ? 1 : 0):0;

	PROFILER_RESET(intra_luma_generate_prediction)
	fill_reference_samples(et, ctu, curr_partition_info, curr_adi_size, decoded_buff-decoded_buff_stride-1, decoded_buff_stride, curr_part_size, Y_COMP, is_filtered);//create filtered and non filtered adi buf

	//predIntraLumaAng
	if(cu_mode== PLANAR_IDX)
		et->funcs->create_intra_planar_prediction(et, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, curr_part_size_shift);//creamos el array de prediccion planar
	else
		et->funcs->create_intra_angular_prediction(et, ctu, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, cu_mode, TRUE);//creamos el array de prediccion angular
	PROFILER_ACCUMULATE(intra_luma_generate_prediction)

	PROFILER_RESET(intra_luma_predict)
	et->funcs->predict(orig_buff, orig_buff_stride, pred_buff, pred_buff_stride, residual_buff, residual_buff_stride, curr_part_size);
	PROFILER_ACCUMULATE(intra_luma_predict)

	//2D -> 1D buffer
	PROFILER_RESET(intra_luma_tr)
	et->funcs->transform(et->bit_depth, residual_buff, et->pred_aux_buff, residual_buff_stride, curr_part_size, curr_part_size, curr_part_size_shift, curr_part_size_shift, cu_mode, quant_buff);
	PROFILER_ACCUMULATE(intra_luma_tr)

	PROFILER_RESET(intra_luma_q)
	et->funcs->quant(et, et->pred_aux_buff, quant_buff, curr_scan_mode, curr_depth, Y_COMP, cu_mode, 1, curr_sum, curr_part_size, per, rem);
	curr_partition_info->sum = *curr_sum;
	PROFILER_ACCUMULATE(intra_luma_q)

	curr_partition_info->intra_cbf[Y_COMP] = (( *curr_sum ? 1 : 0 ) << (curr_depth-depth+(part_size_type==SIZE_NxN)));// + (part_size_type == SIZE_NxN)));
	curr_partition_info->intra_tr_idx = (curr_depth-depth+(part_size_type==SIZE_NxN));
	curr_partition_info->intra_mode[Y_COMP] = cu_mode;
	if(et->rd_mode == RD_FULL)
	{
		memset(&et->cbf_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], (( curr_partition_info->sum ? 1 : 0 ) << (curr_depth-depth+(part_size_type==SIZE_NxN))), curr_partition_info->num_part_in_cu*sizeof(cbf_buff[0]));//(width*width)>>4 num parts of 4x4 in partition
		memset(&et->tr_idx_buffs[curr_depth][curr_partition_info->abs_index], (curr_depth-depth+(part_size_type==SIZE_NxN)), curr_partition_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));//(width*width)>>4 num parts of 4x4 in partition
		memset(&et->intra_mode_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], cu_mode, curr_partition_info->num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));//(width*width)>>4 num parts of 4x4 in partition
	}

	if(curr_partition_info->sum)
	{
		PROFILER_RESET(intra_luma_iq)
		et->funcs->inv_quant(et, quant_buff, iquant_buff, curr_depth, Y_COMP, 1, curr_part_size, per, rem);
		PROFILER_ACCUMULATE(intra_luma_iq)

		//1D ->2D buffer
		et->funcs->itransform(et->bit_depth, residual_buff, iquant_buff, residual_buff_stride, curr_part_size, curr_part_size, cu_mode, et->pred_aux_buff);
		PROFILER_RESET(intra_luma_itr)
		PROFILER_ACCUMULATE(intra_luma_itr)

		PROFILER_RESET(intra_luma_recon_ssd)
		et->funcs->reconst(pred_buff, pred_buff_stride, residual_buff, residual_buff_stride, decoded_buff, decoded_buff_stride, curr_part_size);
	}
	else
	{
		PROFILER_RESET(intra_luma_recon_ssd)
		et->funcs->reconst(pred_buff, pred_buff_stride, quant_buff, 0, decoded_buff, decoded_buff_stride, curr_part_size);//quant buff is full of zeros - a memcpy could do
	}

	ssd_ = et->funcs->ssd(orig_buff, orig_buff_stride, decoded_buff, decoded_buff_stride, curr_part_size);
	PROFILER_ACCUMULATE(intra_luma_recon_ssd)
	return ssd_;
}



#define NUM_SEARCH_LOOPS	4
int search_points[NUM_SEARCH_LOOPS][5] =	{{ 0,  1,  0,  8, 16},//Planar and DC
											{  2, 10, 16, 22, 30},//{  0,  8, 16, 24, 32},//{  4, 12, 20, 28, 16},
											{ -4, -2,  2,  4,  0},
											{ -1,  1,  0,  0,  0}};
int num_search_points[NUM_SEARCH_LOOPS] =	{2, 5, 4, 2};//{2, 4, 4, 2};


#define PRED_MODE_INVALID	100
int homer_loop1_motion_intra(henc_thread_t* et, ctu_info_t* ctu, ctu_info_t* ctu_rd,cu_partition_info_t* curr_partition_info, int16_t *pred_buff, int pred_buff_stride, uint8_t *orig_buff, int orig_buff_stride, int16_t *decoded_buff, int decoded_buff_stride, int depth, int curr_depth, int curr_part_size, int curr_part_size_shift, int part_size_type, int curr_adi_size, int best_pred_modes[3], double best_pred_cost[3])
{
	int preds[3] = {-1,-1,-1};
	int preds_aux[3];
	int num_preds;
	int nloops, n;
	int cu_mode, best_cu_mode = 18, new_best_cu_mode = 18;
	int diff;
	int is_filtered;
	double distortion;
	double cost, best_cost = MAX_COST;
	int min_mode = 0;
	int max_mode = 1;
	int best_bit_cost=0;
	best_pred_modes[0] = best_pred_modes[1] = best_pred_modes[2] = PRED_MODE_INVALID;

	fill_reference_samples(et, ctu, curr_partition_info, curr_adi_size, decoded_buff-decoded_buff_stride-1, decoded_buff_stride, curr_part_size, Y_COMP, TRUE);//create filtered and non filtered adi buf

	ctu_rd->intra_mode[Y_COMP] = et->intra_mode_buffs[Y_COMP][curr_depth];//for rd
//	ctu_rd->pred_mode
	num_preds = get_intra_dir_luma_predictor(ctu_rd, curr_partition_info, preds, NULL);  

	new_best_cu_mode = best_cu_mode = 0;
	best_cost = MAX_COST;
	for(nloops=0;nloops<NUM_SEARCH_LOOPS;nloops++)
	{
		uint bit_cost = 0;

		if(nloops==1)
		{
			best_cu_mode = 2;
			min_mode = 2;
			max_mode = 34;
		}

		for(n=0;n<num_search_points[nloops];n++)
		{
			int inv_depth = (et->max_cu_size_shift - curr_depth);
			cu_mode = best_cu_mode+search_points[nloops][n];
			if(cu_mode<min_mode || cu_mode>max_mode)
			{
				continue;
			}
			diff = min(abs(cu_mode - HOR_IDX), abs(cu_mode - VER_IDX));
			is_filtered = (cu_mode!=DC_IDX) && ((diff > intra_filter[inv_depth - 2]) ? 1 : 0);

			if(cu_mode== PLANAR_IDX)
				et->funcs->create_intra_planar_prediction(et, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, curr_part_size_shift);//creamos el array de prediccion planar
			else
				et->funcs->create_intra_angular_prediction(et, ctu, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, cu_mode, TRUE);//creamos el array de prediccion angular

			PROFILER_RESET(intra_luma_bucle1_sad)
			distortion = (double)et->funcs->sad(orig_buff, orig_buff_stride, pred_buff, pred_buff_stride,curr_part_size);//R-D
			PROFILER_ACCUMULATE(intra_luma_bucle1_sad)
			cost = distortion;
			
			if(et->rd_mode==RD_FULL)
			{
				if(preds[0]==cu_mode || preds[1]==cu_mode || preds[2]==cu_mode)
				{
					memcpy(preds_aux, preds, sizeof(preds));
					bit_cost = fast_rd_estimate_bits_intra_luma_mode(et, curr_partition_info, depth-(part_size_type==SIZE_NxN), cu_mode, preds_aux, num_preds);
				}
				else
				{
					bit_cost = 6;//this introduces some error but gives good results
				}
				cost += bit_cost*et->rd.sqrt_lambda;
			}
			else if(et->rd_mode==RD_FAST)
			{
				if(preds[0]==cu_mode || preds[1]==cu_mode || preds[2]==cu_mode)
					bit_cost = 1;
				else
					bit_cost = 12;
				cost += bit_cost*et->rd.sqrt_lambda;
			}

			if (cost<best_cost)
			{

				if(cost<best_cost)// || (cost==best_cost && new_best_cu_mode<2 && (cu_mode>1)))
				{
					best_cost = cost;
					new_best_cu_mode = cu_mode;
					best_bit_cost = bit_cost;
				}
			}

//			homer_update_cand_list( cu_mode, cost, 3, best_pred_modes, best_pred_cost);
		}//end for(cu_mode=0;cu_mode<NUM_LUMA_MODES;cu_mode++)
		best_cu_mode = new_best_cu_mode;
	}

	best_pred_modes[0]=best_cu_mode;
	best_pred_cost[0]=best_cost;
	return best_bit_cost;
}



void hm_loop1_motion_intra(henc_thread_t* et, ctu_info_t* ctu, ctu_info_t* ctu_rd,cu_partition_info_t* curr_partition_info, int16_t *pred_buff, int pred_buff_stride, uint8_t *orig_buff, int orig_buff_stride, int16_t *decoded_buff, int decoded_buff_stride, int depth, int curr_depth, int curr_part_size, int curr_part_size_shift, int part_size_type, int curr_adi_size, int best_pred_modes[3], double best_pred_cost[3])
{
	int cu_mode;
	int diff;
	int is_filtered;
	double distortion;
	uint cost;
	fill_reference_samples(et, ctu, curr_partition_info, curr_adi_size, decoded_buff-decoded_buff_stride-1, decoded_buff_stride, curr_part_size, Y_COMP, TRUE);//create filtered and non filtered adi buf
	//64x64 intra pred
	for(cu_mode=0;cu_mode<NUM_LUMA_MODES;cu_mode++)
	{
		//check if adi is needed raw or filtered
		int inv_depth = (et->max_cu_size_shift - curr_depth);
		diff = min(abs(cu_mode - HOR_IDX), abs(cu_mode - VER_IDX));
		is_filtered = (cu_mode!=DC_IDX) && ((diff > intra_filter[inv_depth - 2]) ? 1 : 0);//is_luma?((diff > intra_filter[inv_depth - 2]) ? 1 : 0):0;

		//creamos el buffer de prediccion
		if(cu_mode== PLANAR_IDX)
			et->funcs->create_intra_planar_prediction(et, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, curr_part_size_shift);//creamos el array de prediccion planar
		else
			et->funcs->create_intra_angular_prediction(et, ctu, pred_buff, pred_buff_stride, (is_filtered?et->adi_filtered_pred_buff:et->adi_pred_buff), curr_adi_size, curr_part_size, cu_mode, TRUE);//creamos el array de prediccion angular

		//rd (sad or hadamard sad)
		PROFILER_RESET(intra_luma_bucle1_sad)
		distortion = (double)et->funcs->sad(orig_buff, orig_buff_stride, pred_buff, pred_buff_stride,curr_part_size);//R-D
		PROFILER_ACCUMULATE(intra_luma_bucle1_sad)
		cost = (uint32_t)distortion;
//#ifdef COMPUTE_RD

		if(et->rd_mode != RD_DIST_ONLY)
		{
			int32_t bit_cost;
			//info for rd
			memset(&et->intra_mode_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], cu_mode, curr_partition_info->num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));//(width*width)>>4 num parts of 4x4 in partition
			ctu_rd->intra_mode[Y_COMP] = et->intra_mode_buffs[Y_COMP][curr_depth];//for rd
			bit_cost = rd_estimate_bits_intra_mode(et, ctu_rd, curr_partition_info, depth-(part_size_type==SIZE_NxN), TRUE);
			cost += (uint32_t) (bit_cost*et->rd.sqrt_lambda);
		}
		hm_update_cand_list( cu_mode, cost, 3, best_pred_modes, best_pred_cost);
	}//end for(cu_mode=0;cu_mode<NUM_LUMA_MODES;cu_mode++)
}



//results are consolidated in the initial depth with which the function is called
uint32_t encode_intra_luma(henc_thread_t* et, ctu_info_t* ctu, int gcnt, int depth, int part_position, PartSize part_size_type)
{
	int k;
	int cu_mode, best_mode;
	double distortion = 0.;
	double cost, best_cost;
	slice_t *currslice = &et->enc_engine->current_pict.slice;
	ctu_info_t *ctu_rd = et->ctu_rd;
	int pred_buff_stride, orig_buff_stride, decoded_buff_stride;
	uint8_t *orig_buff;
	int16_t *pred_buff, *decoded_buff;
//	uint8_t *cbf_buff = NULL;//, *best_cbf_buff = NULL, *best_cbf_buff2 = NULL;
	int num_mode_candidates = 3;
	int best_pred_modes[3];
	double best_pred_cost[3]= {DOUBLE_MAX,DOUBLE_MAX,DOUBLE_MAX};
	wnd_t *decoded_wnd = NULL;//, *resi_wnd = NULL;

	int curr_part_size, curr_part_size_shift;
	int curr_adi_size;
	int curr_part_x, curr_part_y;
	int curr_depth = depth;
	cu_partition_info_t*	parent_part_info;
	cu_partition_info_t*	curr_partition_info;
	int curr_sum = 0, best_sum;
//	int num_part_in_cu;
	int partition_cost;
	int cu_min_tu_size_shift;
	int depth_state[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	int max_tr_depth, max_tr_processing_depth;
	int initial_state, end_state;
//	int cbf_split[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	uint acc_cost[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	int bitcost_cu_mode;
	int log2cu_size;
	int qp;

	curr_partition_info = &ctu->partition_list[et->partition_depth_start[curr_depth]]+part_position;

	qp = curr_partition_info->qp;

	parent_part_info = curr_partition_info->parent;

	curr_depth = curr_partition_info->depth;
	curr_part_x = curr_partition_info->x_position;
	curr_part_y = curr_partition_info->y_position;
	curr_part_size = curr_partition_info->size;
	curr_part_size_shift = et->max_cu_size_shift-curr_depth;
	curr_adi_size = 2*2*curr_part_size+1;

	pred_buff_stride = WND_STRIDE_2D(et->prediction_wnd, Y_COMP);
	pred_buff = WND_POSITION_2D(int16_t *, et->prediction_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);
	orig_buff_stride = WND_STRIDE_2D(et->curr_mbs_wnd, Y_COMP);
	orig_buff = WND_POSITION_2D(uint8_t *, et->curr_mbs_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);

	decoded_wnd = et->decoded_mbs_wnd[depth+1];	
	decoded_buff_stride = WND_STRIDE_2D(*decoded_wnd, Y_COMP);
	decoded_buff = WND_POSITION_2D(int16_t *, *decoded_wnd, Y_COMP, curr_part_x, curr_part_y, gcnt, et->ctu_width);

	if(et->rd_mode==RD_FULL)
	{
		memset(&ctu_rd->part_size_type[curr_partition_info->abs_index], part_size_type, curr_partition_info->num_part_in_cu*sizeof(ctu_rd->part_size_type[0]));//(width*width)>>4 num parts of 4x4 in partition
		memset(&ctu_rd->pred_depth[curr_partition_info->abs_index], depth-(part_size_type==SIZE_NxN), curr_partition_info->num_part_in_cu*sizeof(ctu_rd->part_size_type[0]));//(width*width)>>4 num parts of 4x4 in partition
	}

	PROFILER_RESET(intra_luma_bucle1)
#ifdef COMPUTE_AS_HM
	hm_loop1_motion_intra(et, ctu, ctu_rd, curr_partition_info, pred_buff, pred_buff_stride, orig_buff, orig_buff_stride, decoded_buff, decoded_buff_stride, depth, curr_depth, curr_part_size, curr_part_size_shift, part_size_type, curr_adi_size, best_pred_modes, best_pred_cost);
#else
	bitcost_cu_mode = homer_loop1_motion_intra(et, ctu, ctu_rd, curr_partition_info, pred_buff, pred_buff_stride, orig_buff, orig_buff_stride, decoded_buff, decoded_buff_stride, depth, curr_depth, curr_part_size, curr_part_size_shift, part_size_type, curr_adi_size, best_pred_modes, best_pred_cost);
#endif

	PROFILER_ACCUMULATE(intra_luma_bucle1)

	if(depth==0 && et->max_cu_size == MAX_CU_SIZE)
	{
		parent_part_info = &ctu->partition_list[et->partition_depth_start[depth]];
		curr_partition_info = parent_part_info->children[0];
		initial_state = part_position;
		end_state = initial_state+4;
	}
	else
	{
		curr_partition_info = &ctu->partition_list[et->partition_depth_start[curr_depth]]+part_position;
		parent_part_info = curr_partition_info->parent;
		initial_state = part_position & 0x3;
		end_state = initial_state+1;
	}

	best_cost = INT_MAX;
	curr_depth = curr_partition_info->depth;
	memset(depth_state, 0, sizeof(depth_state));

	num_mode_candidates = 3;
	if(best_pred_modes[2] == PRED_MODE_INVALID)//for homer prediction
		num_mode_candidates--;

	PROFILER_RESET(intra_luma_bucle2)

#ifdef COMPUTE_AS_HM
	//choose one mode from prediction mode candidate list
	for(k=0;k<num_mode_candidates;k++)
//	for(k=num_mode_candidates-1;k>=0;k--)
	{
//		wnd_t *quant_wnd = et->transform_quant_wnd[curr_depth];
//		decoded_wnd = et->decoded_mbs_wnd[curr_depth];
//		cbf_buff = et->cbf_buffs[Y_COMP][curr_depth];

		cu_mode = best_pred_modes[k];
		cost = 0;
		depth_state[curr_depth] = initial_state;

		while(depth_state[curr_depth]!=end_state)
		{
			int num_part_in_cu;
			curr_partition_info = (parent_part_info==NULL)?curr_partition_info:parent_part_info->children[depth_state[curr_depth]];//if cu_size=64 we process 4 32x32 partitions, else just the curr_partition
			curr_partition_info->qp = qp;
			curr_depth = curr_partition_info->depth;
			num_part_in_cu =  curr_partition_info->num_part_in_cu;

			partition_cost = encode_intra_cu(et, ctu, curr_partition_info, depth, cu_mode, part_size_type, &curr_sum, gcnt);//depth = prediction depth

			cost += partition_cost;
			if(et->rd_mode==RD_FULL)				//rd
			{
				uint bit_cost = 0;
//				memset(&et->intra_mode_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], cu_mode, curr_partition_info->num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));
//				memset(&et->cbf_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], (( curr_sum ? 1 : 0 ) << (curr_depth-depth+(part_size_type==SIZE_NxN))), curr_partition_info->num_part_in_cu*sizeof(et->cbf_buffs[Y_COMP][curr_depth][0]));
//				memset(&et->tr_idx_buffs[curr_depth][curr_partition_info->abs_index], (curr_depth-depth+(part_size_type==SIZE_NxN)), curr_partition_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
				ctu_rd->intra_mode[Y_COMP] = et->intra_mode_buffs[Y_COMP][curr_depth];
				ctu_rd->cbf[Y_COMP] = et->cbf_buffs[Y_COMP][curr_depth];
				ctu_rd->tr_idx = et->tr_idx_buffs[curr_depth];
				ctu_rd->coeff_wnd = et->transform_quant_wnd[curr_depth+1];
				bit_cost = rd_get_intra_bits_qt(et, ctu_rd, curr_partition_info, depth, TRUE, gcnt);

				cost += bit_cost*et->rd.lambda+.5;
			}
			if(num_part_in_cu>1 && cost > best_cost)
			{
				break;
			}
			depth_state[curr_depth]++;
		}

		if(cost < best_cost)
		{
			best_cost = cost;
			best_mode = cu_mode;
			best_sum = curr_sum;
		}
	}
	PROFILER_ACCUMULATE(intra_luma_bucle2)


	if(part_size_type==SIZE_NxN && best_mode == cu_mode && curr_partition_info->size==MIN_TU_SIZE_SHIFT)
	{
		curr_partition_info->cost = (uint32_t)best_cost;
		curr_partition_info->sum = best_sum;
		if(et->rd_mode!=RD_FULL)//if rd is avaliable, this has already been set
		{
			memset(&et->cbf_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], curr_partition_info->intra_cbf[Y_COMP], curr_partition_info->num_part_in_cu*sizeof(et->cbf_buffs[Y_COMP][curr_depth][0]));//(width*width)>>4 num parts of 4x4 in partition
			memset(&et->tr_idx_buffs[curr_depth][curr_partition_info->abs_index], curr_partition_info->intra_tr_idx, curr_partition_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));//(width*width)>>4 num parts of 4x4 in partition
			memset(&et->intra_mode_buffs[Y_COMP][curr_depth][curr_partition_info->abs_index], curr_partition_info->intra_mode[Y_COMP], curr_partition_info->num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));//(width*width)>>4 num parts of 4x4 in partition
		}
		return curr_partition_info->cost;
	}
	cu_mode = best_mode;
#else	//#ifdef COMPUTE_AS_HM
	cu_mode = best_pred_modes[0];//best_mode;
#endif
	if(depth==0 && et->max_cu_size == MAX_CU_SIZE)
	{
		parent_part_info = &ctu->partition_list[et->partition_depth_start[depth]];
		curr_partition_info = parent_part_info->children[0];
		parent_part_info->cost = INT_MAX;//the parent is consolidated if 2Nx2N
		initial_state = part_position & 0x3;//initial_state = 0;
		end_state = initial_state;//end_state = 1;
	}
	else
	{
		curr_partition_info = &ctu->partition_list[et->partition_depth_start[curr_depth]]+part_position;
		parent_part_info = curr_partition_info->parent;
		initial_state = part_position & 0x3;
		end_state = initial_state+1;
	}

	curr_depth = curr_partition_info->depth;

	max_tr_depth = et->max_intra_tr_depth;

	log2cu_size = et->max_cu_size_shift-(depth-(part_size_type==SIZE_NxN));

	if(log2cu_size < et->min_tu_size_shift+et->max_intra_tr_depth-1+(part_size_type==SIZE_NxN))
		cu_min_tu_size_shift = et->min_tu_size_shift;
	else
	{
		cu_min_tu_size_shift = log2cu_size - (max_tr_depth - 1 + (part_size_type==SIZE_NxN));
		if(cu_min_tu_size_shift>MAX_TU_SIZE_SHIFT)
			cu_min_tu_size_shift=MAX_TU_SIZE_SHIFT;
	}

#ifdef COMPUTE_AS_HM
	max_tr_processing_depth = et->max_cu_size_shift-cu_min_tu_size_shift;
#else
	max_tr_processing_depth = et->max_cu_size_shift-cu_min_tu_size_shift;
	if(et->performance_mode >= PERF_FAST_COMPUTATION)
		max_tr_processing_depth = (depth+2<=max_tr_processing_depth)?depth+2:((depth+1<=max_tr_processing_depth)?depth+1:max_tr_processing_depth);
#endif

	memset(acc_cost, 0, sizeof(acc_cost));
	memset(depth_state, 0, sizeof(depth_state));

	depth_state[curr_depth] = initial_state;

	PROFILER_RESET(intra_luma_bucle3)
	while(curr_depth!=depth || depth_state[curr_depth]!=end_state)
	{		
		uint bit_cost = 0;
		curr_partition_info = (parent_part_info==NULL)?curr_partition_info:parent_part_info->children[depth_state[curr_depth]];//if cu_size=64 we process 4 32x32 partitions, else just the curr_partition
		curr_partition_info->qp = qp;
		curr_depth = curr_partition_info->depth;

		curr_partition_info->distortion = encode_intra_cu(et, ctu, curr_partition_info, depth, cu_mode, part_size_type, &curr_sum, gcnt);//depth = prediction depth
		curr_partition_info->sum = curr_sum;

		curr_partition_info->cost = curr_partition_info->distortion;

		acc_cost[curr_depth] += curr_partition_info->distortion;

		if(et->rd_mode == RD_FULL && (curr_depth < max_tr_processing_depth || curr_depth==depth))
		{
			//rd	
			ctu_rd->cbf[Y_COMP] = et->cbf_buffs[Y_COMP][curr_depth];
			ctu_rd->tr_idx = et->tr_idx_buffs[curr_depth];
			ctu_rd->intra_mode[Y_COMP] = et->intra_mode_buffs[Y_COMP][curr_depth];
			ctu_rd->coeff_wnd = et->transform_quant_wnd[curr_depth+1];

			bit_cost = rd_get_intra_bits_qt(et, ctu_rd, curr_partition_info, depth, TRUE, gcnt);
			curr_partition_info->cost += (uint32_t) (bit_cost*et->rd.lambda+.5);		
			acc_cost[curr_depth] += (uint32_t) (bit_cost*et->rd.lambda+.5);
		}
		depth_state[curr_depth]++;

		//HM order
		if(curr_depth < max_tr_processing_depth)//go one level down
		{
			curr_depth++;
			parent_part_info = curr_partition_info;
		}
		else if(depth_state[curr_depth]==4)//consolidate 
		{	
			while(depth_state[curr_depth]==4 && (curr_depth > (depth)))
			{
				uint sum = parent_part_info->children[0]->sum+parent_part_info->children[1]->sum+parent_part_info->children[2]->sum+parent_part_info->children[3]->sum;
				distortion = parent_part_info->children[0]->distortion+parent_part_info->children[1]->distortion+parent_part_info->children[2]->distortion+parent_part_info->children[3]->distortion;
				cost = distortion;

				depth_state[curr_depth] = 0;
				if(et->rd_mode == RD_FULL)
				{
					//rd
					ctu_rd->cbf[Y_COMP] = et->cbf_buffs[Y_COMP][curr_depth];
					ctu_rd->tr_idx = et->tr_idx_buffs[curr_depth];
					ctu_rd->coeff_wnd = et->transform_quant_wnd[curr_depth+1];
					bit_cost = rd_get_intra_bits_qt(et, ctu_rd, parent_part_info, depth, TRUE, gcnt);
					cost += (uint32_t) (bit_cost*et->rd.lambda+.5);
				}

#ifndef COMPUTE_AS_HM
				if((et->rd_mode != RD_FAST && cost < parent_part_info->cost) || ((et->rd_mode == RD_FAST)  && 1.25*(cost + 45*sum) < (parent_part_info->cost+45*parent_part_info->sum)))//1.25*cost < parent_part_info->cost))
#else
				if(cost < parent_part_info->cost)
#endif
				{
					acc_cost[parent_part_info->depth]+= (uint32_t) (cost - parent_part_info->cost);
					parent_part_info->cost = (uint32_t)cost;
					parent_part_info->distortion = (uint32_t)distortion;
					parent_part_info->sum = sum;
//					parent_part_info->mode = cu_mode;

					//consolidate in parent
//					memcpy(&et->cbf_buffs[Y_COMP][parent_part_info->depth][parent_part_info->abs_index], &et->cbf_buffs[Y_COMP][curr_depth][parent_part_info->abs_index], parent_part_info->num_part_in_cu*sizeof(cbf_buff[0]));
//					memcpy(&et->intra_mode_buffs[Y_COMP][parent_part_info->depth][parent_part_info->abs_index], &et->intra_mode_buffs[Y_COMP][curr_depth][parent_part_info->abs_index], parent_part_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
//					memcpy(&et->tr_idx_buffs[parent_part_info->depth][parent_part_info->abs_index], &et->tr_idx_buffs[curr_depth][parent_part_info->abs_index], parent_part_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));

					if(curr_depth == max_tr_processing_depth)	//create cbf and tr_idx buffs
					{
						int nchild;
						uint cbf_split;
						int tr_mask = 0x1<<(curr_depth-depth+(part_size_type == SIZE_NxN));

						cbf_split = (parent_part_info->children[0]->intra_cbf[Y_COMP]&tr_mask)|(parent_part_info->children[1]->intra_cbf[Y_COMP]&tr_mask)|(parent_part_info->children[2]->intra_cbf[Y_COMP]&tr_mask)|(parent_part_info->children[3]->intra_cbf[Y_COMP]&tr_mask);
						cbf_split>>=1;//<<=(curr_depth-depth-1);
						for(nchild=0;nchild<4;nchild++)
						{
							cu_partition_info_t *cu_info = parent_part_info->children[nchild];
							cu_info->intra_cbf[Y_COMP]|=cbf_split;
							memset(&et->cbf_buffs[Y_COMP][depth][cu_info->abs_index], cu_info->intra_cbf[Y_COMP], cu_info->num_part_in_cu*sizeof(et->cbf_buffs[0][0][0]));
							memset(&et->tr_idx_buffs[depth][cu_info->abs_index], cu_info->intra_tr_idx, cu_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
							memset(&et->intra_mode_buffs[Y_COMP][depth][cu_info->abs_index], cu_info->intra_mode[Y_COMP], cu_info->num_part_in_cu*sizeof(et->intra_mode_buffs[0][0][0]));
							//SET_ENC_INFO_BUFFS(et, cu_info, depth+(part_size_type!=SIZE_2Nx2N), cu_info->abs_index, cu_info->num_part_in_cu);//consolidate in prediction depth
						}
					}
					else
					{
						int ll;
						int buff_depth = depth;//+(part_size_type!=SIZE_2Nx2N);
						int abs_index = parent_part_info->abs_index;
						int num_part_in_cu = parent_part_info->num_part_in_cu;
//						uint cbf_split[NUM_PICT_COMPONENTS];
						uint cbf_y;
						int tr_mask = 0x1<<(curr_depth-depth+(part_size_type == SIZE_NxN));
						//int tr_mask = 0x1<<(curr_depth-depth);
						cbf_y = (et->cbf_buffs[Y_COMP][buff_depth][parent_part_info->children[0]->abs_index]&tr_mask)|(et->cbf_buffs[Y_COMP][buff_depth][parent_part_info->children[1]->abs_index]&tr_mask)|(et->cbf_buffs[Y_COMP][buff_depth][parent_part_info->children[2]->abs_index]&tr_mask)|(et->cbf_buffs[Y_COMP][buff_depth][parent_part_info->children[3]->abs_index]&tr_mask);

						//cbf_y <<= (curr_depth-depth-1);
						cbf_y >>=1;
						cbf_y |= ((cbf_y&0xff)<<8) | ((cbf_y&0xff)<<16) | ((cbf_y&0xff)<<24);
						//consolidate cbf flags
						for(ll=(abs_index>>2);ll<((abs_index+num_part_in_cu)>>2);ll++)
						{
							((uint*)(et->cbf_buffs[Y_COMP][buff_depth]))[ll] |= cbf_y;
						}											
					}
					//synchronize buffers for next iterations
					synchronize_motion_buffers_luma(et, parent_part_info, et->transform_quant_wnd[curr_depth+1], et->transform_quant_wnd[curr_depth-1+1], et->decoded_mbs_wnd[curr_depth+1], et->decoded_mbs_wnd[curr_depth-1+1], gcnt);
				}
				else
				{
					memset(&et->cbf_buffs[Y_COMP][depth][parent_part_info->abs_index], parent_part_info->intra_cbf[Y_COMP], parent_part_info->num_part_in_cu*sizeof(et->cbf_buffs[0][0][0]));
					memset(&et->tr_idx_buffs[depth][parent_part_info->abs_index], parent_part_info->intra_tr_idx, parent_part_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
					memset(&et->intra_mode_buffs[Y_COMP][depth][parent_part_info->abs_index], parent_part_info->intra_mode[Y_COMP], parent_part_info->num_part_in_cu*sizeof(et->intra_mode_buffs[0][0][0]));
					synchronize_reference_buffs(et, parent_part_info, et->decoded_mbs_wnd[curr_depth-1+1], et->decoded_mbs_wnd[curr_depth+1], gcnt);	
				}

				acc_cost[curr_depth] = 0;
				curr_depth--;
				parent_part_info = parent_part_info->parent;
			}

			if(curr_depth+2 <= max_tr_processing_depth)
			{
				int aux_depth, aux_abs_index, aux_num_part_in_cu;
				cu_partition_info_t*	aux_partition_info = (parent_part_info!=NULL)?parent_part_info->children[(depth_state[curr_depth]+3)&0x3]:&ctu->partition_list[0];
				aux_abs_index = aux_partition_info->abs_index;
				aux_num_part_in_cu = aux_partition_info->num_part_in_cu;
				for(aux_depth=curr_depth+2;aux_depth<=max_tr_processing_depth;aux_depth++)//+1 pq el nivel superior esta ya sincronizado y tenemos que sincronizar los siguientes
				{
					synchronize_reference_buffs(et, aux_partition_info, et->decoded_mbs_wnd[curr_depth+1], et->decoded_mbs_wnd[aux_depth+1], gcnt);	
					if(et->rd_mode==RD_FULL)
					{
						memcpy(&et->intra_mode_buffs[Y_COMP][aux_depth][aux_abs_index], &et->intra_mode_buffs[Y_COMP][depth][aux_abs_index], aux_num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));
						memcpy(&et->cbf_buffs[Y_COMP][aux_depth][aux_abs_index], &et->cbf_buffs[Y_COMP][depth][aux_abs_index], aux_num_part_in_cu*sizeof(et->cbf_buffs[Y_COMP][0][0]));
						memcpy(&et->tr_idx_buffs[aux_depth][aux_abs_index], &et->tr_idx_buffs[depth][aux_abs_index], aux_num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
					}
				}
			}
		}
	}
	PROFILER_ACCUMULATE(intra_luma_bucle3)

	curr_partition_info = &ctu->partition_list[et->partition_depth_start[depth]]+part_position;
	curr_part_size_shift = et->max_cu_size_shift-curr_partition_info->depth;

	if(depth == max_tr_processing_depth)//if tr_depth==1
	{
		memset(&et->cbf_buffs[Y_COMP][depth][curr_partition_info->abs_index], curr_partition_info->intra_cbf[Y_COMP], curr_partition_info->num_part_in_cu*sizeof(et->cbf_buffs[0][0][0]));
		memset(&et->tr_idx_buffs[depth][curr_partition_info->abs_index], curr_partition_info->intra_tr_idx, curr_partition_info->num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
		memset(&et->intra_mode_buffs[Y_COMP][depth][curr_partition_info->abs_index], curr_partition_info->intra_mode[Y_COMP], curr_partition_info->num_part_in_cu*sizeof(et->intra_mode_buffs[0][0][0]));		
	}

	parent_part_info = curr_partition_info->parent;
	if(part_size_type == SIZE_NxN && (part_position & 0x3)==3)
	{
		int ll;
		int num_part_in_sub_cu = parent_part_info->children[0]->num_part_in_cu;
		int abs_index = parent_part_info->abs_index;
		uint32_t cbf_mask = 0x2;//1<<curr_depth-1;
		uint32_t cbf_split_y = (et->cbf_buffs[Y_COMP][curr_depth][abs_index]&cbf_mask) | (et->cbf_buffs[Y_COMP][curr_depth][abs_index+num_part_in_sub_cu]&cbf_mask) | (et->cbf_buffs[Y_COMP][curr_depth][abs_index+2*num_part_in_sub_cu]&cbf_mask) | (et->cbf_buffs[Y_COMP][curr_depth][abs_index+3*num_part_in_sub_cu]&cbf_mask);
		if(cbf_split_y)
		{
			cbf_split_y = cbf_split_y>>1;
			cbf_split_y = cbf_split_y | (cbf_split_y<<8) | (cbf_split_y<<16) | (cbf_split_y<<24);
			//consolidate cbf flags
			for(ll=abs_index;ll<abs_index+4*num_part_in_sub_cu;ll+=4)
			{
				(*(uint32_t*)&et->cbf_buffs[Y_COMP][curr_depth][ll]) |= cbf_split_y;
			}
		}
	
	}

#ifndef COMPUTE_AS_HM
	if(et->rd_mode != RD_FULL)
	{
		double correction = calc_mv_correction(curr_partition_info->qp, et->enc_engine->avg_dist);//.25+et->enc_engine->avg_dist*et->enc_engine->avg_dist/5000000.;
		return (uint32_t)(curr_partition_info->cost+(bitcost_cu_mode)*correction+.5);//curr_partition_info->qp*correction+.5);//+curr_partition_info->size_chroma*et->rd.lambda+.5;
//		return (curr_partition_info->cost+(bitcost_cu_mode)*curr_partition_info->qp/clip((3500000/(et->enc_engine->avg_dist*et->enc_engine->avg_dist)),.35,4.)+.5);//+curr_partition_info->size_chroma*et->rd.lambda+.5;
	}
	else
#endif
		return curr_partition_info->cost;
}

void consolidate_info_buffers_for_rd(henc_thread_t* et, ctu_info_t* ctu, int dest_depth, int abs_index, int num_part_in_cu)
{
	{
		memcpy(&et->cbf_buffs[Y_COMP][dest_depth][abs_index], &ctu->cbf[Y_COMP][abs_index], num_part_in_cu*sizeof(et->cbf_buffs[Y_COMP][0][0]));
		memcpy(&et->cbf_buffs[U_COMP][dest_depth][abs_index], &ctu->cbf[U_COMP][abs_index], num_part_in_cu*sizeof(et->cbf_buffs[U_COMP][0][0]));
		memcpy(&et->cbf_buffs[V_COMP][dest_depth][abs_index], &ctu->cbf[V_COMP][abs_index], num_part_in_cu*sizeof(et->cbf_buffs[V_COMP][0][0]));
		memcpy(&et->intra_mode_buffs[Y_COMP][dest_depth][abs_index], &ctu->intra_mode[Y_COMP][abs_index], num_part_in_cu*sizeof(et->intra_mode_buffs[Y_COMP][0][0]));
		memcpy(&et->intra_mode_buffs[CHR_COMP][dest_depth][abs_index], &ctu->intra_mode[CHR_COMP][abs_index], num_part_in_cu*sizeof(et->intra_mode_buffs[CHR_COMP][0][0]));
		memcpy(&et->tr_idx_buffs[dest_depth][abs_index], &ctu->tr_idx[abs_index], num_part_in_cu*sizeof(et->tr_idx_buffs[0][0]));
	}
}


uint32_t calc_variance_cu(henc_thread_t* et, cu_partition_info_t *curr_cu_info)
{
	int gcnt = 0;
	int orig_buff_stride = WND_STRIDE_2D(et->curr_mbs_wnd, Y_COMP);
	uint8_t *orig_buff = WND_POSITION_2D(uint8_t *, et->curr_mbs_wnd, Y_COMP, curr_cu_info->x_position, curr_cu_info->y_position, 0, et->ctu_width);
	curr_cu_info->variance_luma = et->funcs->modified_variance(orig_buff, curr_cu_info->size, orig_buff_stride, 1)/(curr_cu_info->size*curr_cu_info->size);//for intra imgs this is done in analyse_intra_recursive_info
	orig_buff_stride = WND_STRIDE_2D(et->curr_mbs_wnd, U_COMP);
	orig_buff = WND_POSITION_2D(uint8_t *, et->curr_mbs_wnd, U_COMP, curr_cu_info->x_position_chroma, curr_cu_info->y_position_chroma, gcnt, et->ctu_width);
	curr_cu_info->variance_chroma = (uint32_t)(1.25*et->funcs->modified_variance(orig_buff, curr_cu_info->size_chroma, orig_buff_stride, 2)/(curr_cu_info->size_chroma*curr_cu_info->size_chroma));
	orig_buff = WND_POSITION_2D(uint8_t *, et->curr_mbs_wnd, V_COMP, curr_cu_info->x_position_chroma, curr_cu_info->y_position_chroma, gcnt, et->ctu_width);
	curr_cu_info->variance_chroma += (uint32_t)(1.25*et->funcs->modified_variance(orig_buff, curr_cu_info->size_chroma, orig_buff_stride, 2)/(curr_cu_info->size_chroma*curr_cu_info->size_chroma));
	curr_cu_info->variance = curr_cu_info->variance_luma + curr_cu_info->variance_chroma;
	return curr_cu_info->variance;
}

void analyse_recursive_info_cu(henc_thread_t* et, cu_partition_info_t *curr_partition_info)
{
	cu_partition_info_t	*parent_part_info;
	int position;
	int depth_state[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	int curr_depth = curr_partition_info->depth;
	int initial_depth = curr_partition_info->depth;

	if(curr_partition_info->parent==NULL)
	{
		position = 0;
		parent_part_info = curr_partition_info;
	}
	else
	{
		position = curr_partition_info->list_index-curr_partition_info->parent->children[0]->list_index;
		parent_part_info = curr_partition_info->parent;	
	}

	depth_state[curr_depth] = position;

	while(curr_depth!=initial_depth || depth_state[curr_depth]!=((position+1)&0x3))
	{
		curr_partition_info->variance = 0;
		curr_partition_info->recursive_split = 0;

		if(curr_partition_info->is_b_inside_frame && curr_partition_info->is_r_inside_frame)
		{
			curr_partition_info->variance = calc_variance_cu(et, curr_partition_info);
		}
		else
			curr_partition_info->recursive_split = 1;

		depth_state[curr_depth]++;

		if(curr_depth<et->max_pred_partition_depth)//if tl is not inside the frame don't analyze the next depths
		{
			curr_depth++;
			parent_part_info = curr_partition_info;
		}
		else if(depth_state[curr_depth]==4)
		{
			while(depth_state[curr_depth]==4 && curr_depth>0)
			{
				int l;
				depth_state[curr_depth] = 0;

				for (l=0;l<4;l++)
				{
//					uint child_variance = .5+((double)curr_depth/5.5)*sqrt((double)parent_part_info->children[l]->variance);//+3*/*et->performance_mode2**/curr_depth;
					uint32_t child_variance = (uint32_t)(.5+((double)curr_depth/4.)*sqrt((double)parent_part_info->children[l]->variance)+3*curr_depth);
					uint32_t parent_variance = (uint32_t)(.5+sqrt((double)parent_part_info->variance));

					if((parent_variance > child_variance) || parent_part_info->children[l]->recursive_split)
					{
						parent_part_info->recursive_split = 1;//entropy simplifies splitting in quads
						break;
					}
				}

				curr_depth--;
				parent_part_info = parent_part_info->parent;
			}				
		}
		if(parent_part_info!=NULL)
			curr_partition_info = parent_part_info->children[depth_state[curr_depth]];
	}
}



uint encode_intra(henc_thread_t* et, ctu_info_t* ctu, int gcnt, int curr_depth, int position, PartSize part_size_type)
{
	uint cost = 0;
	uint cost_luma, cost_chroma;
	if(part_size_type == SIZE_2Nx2N)
	{
		cost_luma = encode_intra_luma(et, ctu, gcnt, curr_depth, position, part_size_type);
		cost_chroma = encode_intra_chroma(et, ctu, gcnt, curr_depth, position, part_size_type);//encode_intra_chroma(et, gcnt, depth, position, part_size_type);	
		cost = cost_luma + cost_chroma;
	}
	else if(part_size_type == SIZE_NxN)
	{
		int n;
		slice_t *currslice = &et->enc_engine->current_pict.slice;
		cu_partition_info_t *curr_cu_info = &ctu->partition_list[et->partition_depth_start[curr_depth]]+position;
		for(n=0;n<4;n++)
		{
			curr_cu_info->qp = hmr_rc_get_cu_qp(et, ctu, curr_cu_info, currslice);
			cost_luma = encode_intra_luma(et, ctu, gcnt, curr_depth, position+n, part_size_type);
			cost += cost_luma;
			curr_cu_info++;
		}
		cost_chroma = encode_intra_chroma(et, ctu, gcnt, curr_depth, position, part_size_type);//encode_intra_chroma(et, gcnt, depth, position, part_size_type);	
		cost += cost_chroma;
	}
	return cost;
}

uint motion_intra_cu(henc_thread_t* et, ctu_info_t* ctu, cu_partition_info_t *init_partition_info)
{
	int gcnt = 0;
	double cost, cost_luma, cost_chroma, best_cost;
	slice_t *currslice = &et->enc_engine->current_pict.slice;
	cu_partition_info_t	*parent_part_info = NULL;
	cu_partition_info_t	*curr_partition_info = init_partition_info;
	int position = 0, initial_position = 0;
	int initial_depth = init_partition_info->depth;
	int curr_depth = init_partition_info->depth;
	PartSize part_size_type = SIZE_2Nx2N;
	wnd_t *quant_wnd = NULL, *decoded_wnd = NULL;
	ctu_info_t *ctu_rd = et->ctu_rd;
	int depth_state[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	uint32_t cost_sum[MAX_PARTITION_DEPTH] = {0,0,0,0,0};
	int abs_index;
	int num_part_in_cu;
	uint8_t cbf_split[NUM_PICT_COMPONENTS];
	int stop_recursion = 0;

	//init rd auxiliar ctu
/*	if(et->rd_mode != RD_DIST_ONLY)
	{
		copy_ctu(ctu, ctu_rd);
		memset(ctu_rd->pred_mode, INTRA_MODE, curr_partition_info->num_part_in_cu*sizeof(ctu_rd->pred_mode[0]));//indicamos que todas las codificaciones son intra
//		ctu_rd->pred_mode = INTRA_MODE;
	}
*/
#ifndef COMPUTE_AS_HM
	if(et->performance_mode != PERF_FULL_COMPUTATION && ((currslice->slice_type == I_SLICE && curr_depth==0) || (currslice->slice_type != I_SLICE)))
		analyse_recursive_info_cu(et, curr_partition_info);
#endif
	if(curr_partition_info->parent==NULL)
	{
		initial_position = 0;
		parent_part_info = curr_partition_info;
	}
	else
	{
		initial_position = curr_partition_info->list_index-curr_partition_info->parent->children[0]->list_index;
		parent_part_info = curr_partition_info->parent;	
	}

	depth_state[curr_depth] = initial_position;

	memset(cbf_split, 0, sizeof(cbf_split));
	while(curr_depth!=initial_depth || depth_state[curr_depth]!=initial_position+1)
	{
		stop_recursion = 0;
		curr_depth = curr_partition_info->depth;
		num_part_in_cu = curr_partition_info->num_part_in_cu;
		abs_index = curr_partition_info->abs_index;
		part_size_type = (curr_depth<et->max_pred_partition_depth)?SIZE_2Nx2N:SIZE_NxN;//
		position = curr_partition_info->list_index - et->partition_depth_start[curr_depth];

		cost_luma = cost_chroma = 0;
		//rc
		curr_partition_info->qp = hmr_rc_get_cu_qp(et, ctu, curr_partition_info, currslice);

		if(curr_partition_info->is_b_inside_frame && curr_partition_info->is_r_inside_frame)//if br (and tl) are inside the frame, process
		{
#ifndef COMPUTE_AS_HM

//			if((et->performance_mode == PERF_FAST_COMPUTATION && curr_partition_info->recursive_split && curr_partition_info->children[0] && curr_partition_info->children[0]->recursive_split && curr_partition_info->children[1]->recursive_split && curr_partition_info->children[2]->recursive_split && curr_partition_info->children[3]->recursive_split) ||
//				(et->performance_mode == PERF_UFAST_COMPUTATION && curr_partition_info->recursive_split))
			if(et->performance_mode != PERF_FULL_COMPUTATION && curr_partition_info->recursive_split)
			{
				cost_luma = MAX_COST;
				cost_chroma = 0;//MAX_COST;
				curr_partition_info->cost = (uint32_t)cost_luma;
			}
			else
#endif
			{
				if(part_size_type == SIZE_2Nx2N)
				{
					//encode
					PROFILER_RESET(intra_luma)
					cost_luma = encode_intra_luma(et, ctu, gcnt, curr_depth, position, part_size_type);
					PROFILER_ACCUMULATE(intra_luma)

					cost_chroma = 0;
					PROFILER_RESET(intra_chroma)
					cost_chroma = encode_intra_chroma(et, ctu, gcnt, curr_depth, position, part_size_type);//encode_intra_chroma(et, gcnt, depth, position, part_size_type);	
					PROFILER_ACCUMULATE(intra_chroma)

					curr_partition_info->cost =(uint32_t) (cost_luma + cost_chroma);
					cost_sum[curr_depth] += curr_partition_info->cost;
					curr_partition_info->prediction_mode = INTRA_MODE;
				}
				else if(part_size_type == SIZE_NxN)
				{
					int n;
					PROFILER_RESET(intra_luma)
					cost_luma = 0;

					for(n=0;n<4;n++)
					{
						curr_partition_info->qp = hmr_rc_get_cu_qp(et, ctu, curr_partition_info, currslice);
						curr_partition_info->cost = encode_intra_luma(et, ctu, gcnt, curr_depth, position+n, part_size_type);
						cost_luma += curr_partition_info->cost;
						cost_sum[curr_depth]+=curr_partition_info->cost;
						curr_partition_info->prediction_mode = INTRA_MODE;
						curr_partition_info++;
					}
					curr_partition_info-=4;
					PROFILER_ACCUMULATE(intra_luma)

					if(cost_luma < parent_part_info->cost && (curr_partition_info->is_b_inside_frame && curr_partition_info->is_r_inside_frame))//esto se tiene que hacer aqui pq si sno se eligen cu_modes distintos para cada una de las 4 particiones
					{
						position = parent_part_info->children[0]->list_index - et->partition_depth_start[curr_depth];
						PROFILER_RESET(intra_chroma)
						cost_chroma = encode_intra_chroma(et, ctu, gcnt, curr_depth, position, part_size_type);//encode_intra_chroma(et, gcnt, depth, position, part_size_type);	
						curr_partition_info->cost += (uint32_t)cost_chroma; 
						cost_sum[curr_depth]+=(uint32_t)cost_chroma;
						PROFILER_ACCUMULATE(intra_chroma)
					}
					depth_state[curr_depth]=3;					
				}
			}
		}
		depth_state[curr_depth]++;

#ifndef COMPUTE_AS_HM
		//This doesn't make sense any more when calculating the 4 NxN cus at one time
		//if this matches, it is useless to continue the recursion. the case where part_size_type != SIZE_NxN is checked at the end of the consolidation loop)
//		if(/*et->performance_mode == PERF_FULL_COMPUTATION && */part_size_type == SIZE_NxN && depth_state[curr_depth]!=4 && cost_sum[curr_depth] > parent_part_info->cost && ctu->partition_list[0].is_b_inside_frame && ctu->partition_list[0].is_r_inside_frame)//parent_part_info->is_b_inside_frame && parent_part_info->is_r_inside_frame)
/*		{
			depth_state[curr_depth]=4;
		}
*/
		//stop recursion
		if(et->performance_mode>PERF_FULL_COMPUTATION && (curr_partition_info->recursive_split==0 || curr_partition_info->cost == 0) && /*curr_depth && */part_size_type != SIZE_NxN && curr_partition_info->is_b_inside_frame && curr_partition_info->is_r_inside_frame)
		{
			int max_processing_depth;// = min(et->max_pred_partition_depth+et->max_intra_tr_depth-1, MAX_PARTITION_DEPTH-1);

			consolidate_prediction_info(et, ctu, ctu_rd, curr_partition_info, curr_partition_info->cost, MAX_COST, FALSE, NULL);

			stop_recursion = 1;

			max_processing_depth = min(et->max_pred_partition_depth+et->max_intra_tr_depth-1, MAX_PARTITION_DEPTH-1);

			if(curr_depth <= max_processing_depth)//el = es para cuando et->max_intra_tr_depth!=4
			{
				int aux_depth;
				cu_partition_info_t*	aux_partition_info = (parent_part_info!=NULL)?parent_part_info->children[(depth_state[curr_depth]+3)&0x3]:&ctu->partition_list[0];
				abs_index = aux_partition_info->abs_index;
				num_part_in_cu  = aux_partition_info->num_part_in_cu;

				for(aux_depth=curr_depth;aux_depth<=max_processing_depth;aux_depth++)
				{
					synchronize_reference_buffs(et, aux_partition_info, et->decoded_mbs_wnd[0], et->decoded_mbs_wnd[aux_depth+1], gcnt);	
					//for rd
					if(et->rd_mode!=RD_DIST_ONLY)
						consolidate_info_buffers_for_rd(et, ctu, aux_depth, abs_index, num_part_in_cu);						
				}
				synchronize_reference_buffs_chroma(et, aux_partition_info, et->decoded_mbs_wnd[0], et->decoded_mbs_wnd[NUM_DECODED_WNDS-1], gcnt);
			}
		}
#endif 
		if(!stop_recursion && curr_depth<et->max_pred_partition_depth && curr_partition_info->is_tl_inside_frame)//depth_state[curr_depth]!=4 is for fast skip//if tl is not inside the frame don't process the next depths
		{
			curr_depth++;
			parent_part_info = curr_partition_info;
		}
		else if(/*stop_recursion || */depth_state[curr_depth]==4)//la consolidation of depth =1 has been done before the loop
		{
			int max_processing_depth;

			while(depth_state[curr_depth]==4 && curr_depth>initial_depth)//>0 pq consolidamos sobre el padre, 
			{
				cost = parent_part_info->children[0]->cost + parent_part_info->children[1]->cost +parent_part_info->children[2]->cost+parent_part_info->children[3]->cost;

				depth_state[curr_depth] = 0;
				best_cost = parent_part_info->cost;

				consolidate_prediction_info(et, ctu, ctu_rd, parent_part_info, (uint32_t)best_cost, (uint32_t)cost, (curr_depth==et->max_pred_partition_depth), cost_sum);

				cost_sum[curr_depth] = 0;
				curr_depth--;
				parent_part_info = parent_part_info->parent;

				if(et->performance_mode == PERF_FULL_COMPUTATION && curr_depth>0 && curr_depth<et->max_pred_partition_depth && depth_state[curr_depth]<4 && ctu->partition_list[0].is_b_inside_frame && ctu->partition_list[0].is_r_inside_frame)
				{
					double totalcost = 0;
					int h;
					for(h=0;h<depth_state[curr_depth];h++)
						totalcost += parent_part_info->children[h]->cost;

					if(totalcost > parent_part_info->cost)//cost of childs is already higher than cost of parent - stop recursion
					{
						depth_state[curr_depth] = 4;
					}
				}
				cost_chroma = 0;//only!=0 if part_size_type == SIZE_NxN
			}

			max_processing_depth = min(et->max_pred_partition_depth+et->max_intra_tr_depth-1, MAX_PARTITION_DEPTH-1);

			if(curr_depth <= max_processing_depth)//el = es para cuando et->max_intra_tr_depth!=4
			{
				int aux_depth;
				cu_partition_info_t*	aux_partition_info = (parent_part_info!=NULL)?parent_part_info->children[(depth_state[curr_depth]+3)&0x3]:&ctu->partition_list[0];
				abs_index = aux_partition_info->abs_index;
				num_part_in_cu  = aux_partition_info->num_part_in_cu;

				for(aux_depth=curr_depth;aux_depth<=max_processing_depth;aux_depth++)
				{
					synchronize_reference_buffs(et, aux_partition_info, et->decoded_mbs_wnd[0], et->decoded_mbs_wnd[aux_depth+1], gcnt);	

					//for rd
					if(et->rd_mode!=RD_DIST_ONLY)
						consolidate_info_buffers_for_rd(et, ctu, aux_depth, abs_index, num_part_in_cu);
				}
				synchronize_reference_buffs_chroma(et, aux_partition_info, et->decoded_mbs_wnd[0], et->decoded_mbs_wnd[NUM_DECODED_WNDS-1], gcnt);
			}
		}

		if(parent_part_info!=NULL)
			curr_partition_info = parent_part_info->children[depth_state[curr_depth]];
	}
	
	curr_partition_info = init_partition_info;//&ctu->partition_list[0];
	abs_index = curr_partition_info->abs_index;
	curr_depth = curr_partition_info->depth;
	num_part_in_cu = curr_partition_info->num_part_in_cu;

	memset(&ctu->mv_ref_idx[REF_PIC_LIST_0][abs_index], -1, num_part_in_cu*sizeof(ctu->mv_ref_idx[0][0]));
	memset(&ctu->pred_mode[abs_index], INTRA_MODE, num_part_in_cu*sizeof(ctu->pred_mode[0]));//signal all partitions as intra
	memset(&ctu->skipped[abs_index], 0, num_part_in_cu*sizeof(ctu->skipped[0]));//signal all partitions as non skipped
	return init_partition_info->cost;//best_cost;
}


uint motion_intra(henc_thread_t* et, ctu_info_t* ctu, int gcnt)
{
	int curr_depth = 0;
	ctu_info_t *ctu_rd = et->ctu_rd;
	cu_partition_info_t	*curr_partition_info = ctu->partition_list;

	//init rd auxiliar ctu
	if(et->rd_mode != RD_DIST_ONLY)
	{
		copy_ctu(ctu, ctu_rd);
		memset(ctu_rd->pred_mode, INTRA_MODE, curr_partition_info->num_part_in_cu*sizeof(ctu_rd->pred_mode[0]));//all cus are intra
	}

	return motion_intra_cu(et, ctu, curr_partition_info);
}

