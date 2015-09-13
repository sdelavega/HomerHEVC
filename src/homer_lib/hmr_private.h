/*****************************************************************************
 * hmr_private.h : homerHEVC encoding library
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


#ifndef __HOMER_HEVC_PRIVATE_H__
#define __HOMER_HEVC_PRIVATE_H__

#include <stdio.h>
#include "hmr_os_primitives.h"
#include "homer_hevc_enc_api.h"

//#define WRITE_REF_FRAMES		1

#define COMPUTE_SSE_FUNCS		1
//#define COMPUTE_AS_HM			1	//to debug against HM
#define DBG_TRACE				1
//#define DBG_TRACE_RESULTS		1
//#define COMPUTE_METRICS			1

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		This is the herarchy to follow

		Headers									coding layer
	---------------								------------
	video parameter set								Gop 
		|											 |
	Sequence parameter set							Picture
		|											 |
	picture_t parameter set							Slice
		|
	 slice header
********************************************************************/

#define DOUBLE_MAX              (1.7e+308)    
#define MAX_COST				(UINT_MAX/8)

#define MIN_QP                      0
#define MAX_QP                      51



#define MAX_NUM_CTUs				8160	//1920*1088 resolution
//#define MAX_MB_GROUP_SIZE		1

#define     MAX_CU_DEPTHS			7		//from 128x128 to 2x2. For scan piramid. 
//#define     MAX_CU_SIZE					(1<<(MAX_CU_DEPTHS))        // maximum allowable size of CU
#define     MAX_CU_SIZE_SHIFT		6								//MAX_CU_DEPTH	
#define     MAX_CU_SIZE			(1<<(MAX_CU_SIZE_SHIFT))	// current maximum allowable size of CU

#define		MAX_PARTITION_DEPTH		5 //de 64x64 a 4x4 ambos inclusive

#define		MIN_TU_SIZE_SHIFT		2		//min size of Transform is 4x4
#define		MAX_TU_SIZE_SHIFT		5		//min size of Transform is 32x32

#define     MAX_NUM_PARTITIONS		256							//(1<<MAX_CU_PARTITIONS_SHIFT)*(1<<MAX_CU_PARTITIONS_SHIFT) - 16 particiones por eje - se corresponde con el peor caso

#define     MOTION_SEARCH_RANGE_X		(128)
#define     MOTION_SEARCH_RANGE_Y		(64)

#define QUANT_DEFAULT_DC	16

#define CU_DQP_TU_CMAX 5                   //max number bins for truncated unary
#define CU_DQP_EG_k 0                      //expgolomb order


//for intra prediction
#define NUM_INTRA_MODES					35
#define NUM_INTRA_CANDIDATES			3


//scannig modes
#define ZIGZAG_SCAN							0     
#define HOR_SCAN							1
#define VER_SCAN							2
#define DIAG_SCAN							3
#define NUM_SCAN_MODES						4

//scaling list modes
#define SCALING_MODE_4x4					0
#define SCALING_MODE_8x8					1
#define SCALING_MODE_16x16					2
#define SCALING_MODE_32x32					3			
#define NUM_SCALING_MODES					4


#define NUM_SCALING_LISTS					6         //list number for quantization matrix
#define NUM_SCALING_REM_LISTS				6         //remainder of QP/6
#define NUM_MAX_MATRIX_SIZE					8		  //max size number for quantization matrix	//MAX_MATRIX_SIZE_NUM en HM

//from HM
#define QUANT_IQUANT_SHIFT					20 // Q(QP%6) * IQ(QP%6) = 2^20
#define QUANT_SHIFT							14 // Q(4) = 2^14
#define SCALE_BITS							15 // Inherited from TMuC, pressumably for fractional bit estimates in RDOQ
#define MAX_TR_DYNAMIC_RANGE				15 // Maximum transform dynamic range (excluding sign bit)


//interpolation filter
#define   INTERPOLATE_HOR		0
#define   INTERPOLATE_VERT		1


//deblock filter
#define   EDGE_VER    0
#define   EDGE_HOR    1

#define DEBLOCK_SMALLEST_BLOCK  8
#define DEFAULT_INTRA_TC_OFFSET 2

//-------------------------init binary encode-------------------------------------
#define NUM_SPLIT_FLAG_CTX            3       ///< number of context models for split flag
#define NUM_SKIP_FLAG_CTX             3       ///< number of context models for skip flag

#define NUM_MERGE_FLAG_EXT_CTX        1       ///< number of context models for merge flag of merge extended
#define NUM_MERGE_IDX_EXT_CTX         1       ///< number of context models for merge index of merge extended

#define NUM_PART_SIZE_CTX             4       ///< number of context models for partition size
//#define NUM_CU_AMP_CTX                1       ///< number of context models for partition size (AMP)
#define NUM_PRED_MODE_CTX             1       ///< number of context models for prediction mode

#define NUM_ADI_CTX                   1       ///< number of context models for intra prediction

#define NUM_CHROMA_PRED_CTX           2       ///< number of context models for intra prediction (chroma)
#define NUM_INTER_DIR_CTX             5       ///< number of context models for inter prediction direction
#define NUM_MV_RES_CTX                2       ///< number of context models for motion vector difference

#define NUM_REF_NO_CTX                2       ///< number of context models for reference index
#define NUM_TRANS_SUBDIV_FLAG_CTX     3       ///< number of context models for transform subdivision flags
#define NUM_QT_CBF_CTX                4       ///< number of context models for QT CBF
#define NUM_QT_ROOT_CBF_CTX           1       ///< number of context models for QT ROOT CBF
#define NUM_DELTA_QP_CTX              3       ///< number of context models for dQP

#define NUM_SIG_CG_FLAG_CTX           2       ///< number of context models for MULTI_LEVEL_SIGNIFICANCE

#define NUM_SIG_FLAG_CTX              42      ///< number of context models for sig flag
#define NUM_SIG_FLAG_CTX_LUMA         27      ///< number of context models for luma sig flag
#define NUM_SIG_FLAG_CTX_CHROMA       15      ///< number of context models for chroma sig flag

#define NUM_CTX_LAST_FLAG_XY          15      ///< number of context models for last coefficient position

#define NUM_ONE_FLAG_CTX              24      ///< number of context models for greater than 1 flag
#define NUM_ONE_FLAG_CTX_LUMA         16      ///< number of context models for greater than 1 flag of luma
#define NUM_ONE_FLAG_CTX_CHROMA        8      ///< number of context models for greater than 1 flag of chroma
#define NUM_ABS_FLAG_CTX               6      ///< number of context models for greater than 2 flag
#define NUM_ABS_FLAG_CTX_LUMA          4      ///< number of context models for greater than 2 flag of luma
#define NUM_ABS_FLAG_CTX_CHROMA        2      ///< number of context models for greater than 2 flag of chroma

#define NUM_MVP_IDX_CTX               1       ///< number of context models for MVP index

#define NUM_SAO_MERGE_FLAG_CTX        1       ///< number of context models for SAO merge flags
#define NUM_SAO_TYPE_IDX_CTX          1       ///< number of context models for SAO type index

#define NUM_TRANSFORMSKIP_FLAG_CTX    1       ///< number of context models for transform skipping 
#define NUM_CU_TRANSQUANT_BYPASS_FLAG_CTX  1 
#define CNU                          154      ///< dummy initialization value for unused context models 'Context model Not Used'


#define NUM_CTXs	(NUM_SPLIT_FLAG_CTX+NUM_SKIP_FLAG_CTX+NUM_MERGE_FLAG_EXT_CTX+NUM_MERGE_IDX_EXT_CTX	\
					+NUM_PART_SIZE_CTX+NUM_PRED_MODE_CTX+NUM_ADI_CTX+NUM_CHROMA_PRED_CTX+NUM_DELTA_QP_CTX	\
					+NUM_INTER_DIR_CTX+NUM_REF_NO_CTX+NUM_MV_RES_CTX+2*NUM_QT_CBF_CTX+NUM_TRANS_SUBDIV_FLAG_CTX	\
					+NUM_QT_ROOT_CBF_CTX+2*NUM_SIG_CG_FLAG_CTX+NUM_SIG_FLAG_CTX+2*NUM_CTX_LAST_FLAG_XY	\
					+2*NUM_CTX_LAST_FLAG_XY+NUM_ONE_FLAG_CTX+NUM_ABS_FLAG_CTX+NUM_MVP_IDX_CTX	\
					/*+NUM_CU_AMP_CTX*/+NUM_SAO_MERGE_FLAG_CTX+NUM_SAO_TYPE_IDX_CTX	\
					+2*NUM_TRANSFORMSKIP_FLAG_CTX+NUM_CU_TRANSQUANT_BYPASS_FLAG_CTX)
//-------------------------end binary encode-------------------------------------


typedef int Boolean;

typedef unsigned char	byte;
typedef unsigned char	uint8_t;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef long			int64;
typedef unsigned long	uint64;

#define BSIGN(x)				(x>0?0:1)
#define SIGN(x)					(x<0?-1:1)
#define QP_BITS                 15

//quantified residual access macros
#define FORMAT_PUT_SIGN(s)	(s<<8)
#define FORMAT_GET_SIGN(s)	((s&0x0000ff00)>>8)
#define FORMAT_PUT_RUN	
#define FORMAT_GET_RUN(s)	(s&0x000000ff)
#define FORMAT_PUT_VAL(s)	(s<<16)	
#define FORMAT_GET_VAL(s)	(s>>16)


//Intra modes 
#define PLANAR_IDX				0
#define VER_IDX					26                    // index for intra VERTICAL   mode
#define HOR_IDX					10                    // index for intra HORIZONTAL mode
#define DC_IDX					1                     // index for intra DC mode
#define NUM_CHROMA_MODE			5                     // total number of chroma modes
#define NUM_LUMA_MODES			35                     // total number of chroma modes
#define DM_CHROMA_IDX			36                    // chroma mode index for derived from luma intra mode

#define REG_DCT					65535


#define INTER_MODE				0           
#define INTRA_MODE				1	
#define NONE_MODE				2	


// partition types
typedef enum 
{
  SIZE_2Nx2N,           // symmetric motion partition,  2Nx2N
  SIZE_2NxN,            // symmetric motion partition,  2Nx N
  SIZE_Nx2N,            // symmetric motion partition,   Nx2N
  SIZE_NxN,             // symmetric motion partition,   Nx N
  SIZE_2NxnU,           // asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
  SIZE_2NxnD,           // asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
  SIZE_nLx2N,           // asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
  SIZE_nRx2N,           // asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
  SIZE_NONE = 15
}PartSize;


typedef enum 
{
  REF_PIC_LIST_0 = 0,   // reference list 0
  REF_PIC_LIST_1 = 1,   // reference list 1
  REF_PIC_LIST_C = 2,   // combined reference list for uni-prediction in B-Slices
  REF_PIC_LIST_X = 100  // special mark
}RefPicList;


typedef enum {
  Y_COMP=0,
  U_COMP,
  V_COMP,
  CHR_COMP = 1,//chroma
  NUM_PICT_COMPONENTS = 3
} PictComponents;
//#define NUM_PICT_COMPONENTS			3				//Y,U,V,PTS,private data, size of private data
//#define NUM_PICT_PARAMS		(NUM_PICT_COMPONENTS+3)	//Y,U,V,PTS,private data, size of private data


#define B_BITS         10    // Number of bits to represent the whole coding interval
#define BITS_TO_LOAD   16
#define HALF           0x01FE      //(1 << (B_BITS-1)) - 2


typedef enum {
  B_SLICE,
  P_SLICE,
  I_SLICE,
  NUM_SLICE_TYPES
} SliceTypes;

#define isIntra(slice_type) (slice_type == I_SLICE)



//AVC Profile IDC definitions
typedef enum {
  FREXT_CAVLC444 = 44,       //!< YUV 4:4:4/14 "CAVLC 4:4:4"
  BASELINE       = 66,       //!< YUV 4:2:0/8  "Baseline"
  MAIN           = 77,       //!< YUV 4:2:0/8  "Main"
  EXTENDED       = 88,       //!< YUV 4:2:0/8  "Extended"
  FREXT_HP       = 100,      //!< YUV 4:2:0/8  "High"
  FREXT_Hi10P    = 110,      //!< YUV 4:2:0/10 "High 10"
  FREXT_Hi422    = 122,      //!< YUV 4:2:2/10 "High 4:2:2"
  FREXT_Hi444    = 244,      //!< YUV 4:4:4/14 "High 4:4:4"
  MVC_HIGH       = 118,      //!< YUV 4:2:0/8  "Multiview High"
  STEREO_HIGH    = 128       //!< YUV 4:2:0/8  "Stereo High"
} ProfileIDC;


//  Available MB modes
typedef enum {
  PSKIP        =  0,
  BSKIP_DIRECT =  0,
  P16x16       =  1,
  P16x8        =  2,
  P8x16        =  3,
  SMB8x8       =  4,
  SMB8x4       =  5,
  SMB4x8       =  6,
  SMB4x4       =  7,
  P8x8         =  8,
  I4MB         =  9,
  I16MB        = 10,
  IBLOCK       = 11,
  SI4MB        = 12,
  I8MB         = 13,
  IPCM         = 14,
  MAXMODE      = 15
} MBModes;

typedef enum {
  VERT_PRED            = 0,
  HOR_PRED             = 1,
  DC_PRED              = 2,
  DIAG_DOWN_LEFT_PRED  = 3,
  DIAG_DOWN_RIGHT_PRED = 4,
  VERT_RIGHT_PRED      = 5,
  HOR_DOWN_PRED        = 6,
  VERT_LEFT_PRED       = 7,
  HOR_UP_PRED          = 8
} I4x4PredModes;

// 16x16 intra prediction modes
typedef enum {
  VERT_PRED_16   = 0,
  HOR_PRED_16    = 1,
  DC_PRED_16     = 2,
  PLANE_16       = 3
} I16x16PredModes;

// 8x8 chroma intra prediction modes
typedef enum {
  DC_PRED_8     =  0,
  HOR_PRED_8    =  1,
  VERT_PRED_8   =  2,
  PLANE_8       =  3
} I8x8PredModes;

//! definition of H.264 syntax elements
typedef enum
{
  SE_HEADER,
  SE_PTYPE,
  SE_MBTYPE,
  SE_REFFRAME,
  SE_INTRAPREDMODE,
  SE_MVD,
  SE_CBP,
  SE_LUM_DC_INTRA,
  SE_CHR_DC_INTRA,
  SE_LUM_AC_INTRA,
  SE_CHR_AC_INTRA,
  SE_LUM_DC_INTER,
  SE_CHR_DC_INTER,
  SE_LUM_AC_INTER,
  SE_CHR_AC_INTER,
  SE_DELTA_QUANT,
  SE_BFRAME,
  SE_EOS,
  SE_MAX_ELEMENTS = 20 //!< number of maximum syntax elements
} SE_type;             // substituting the definitions in elements.h


#define IS_INTRA(mode)    (mode==SI4MB || mode==I4MB || mode==I16MB || mode==I8MB || mode==IPCM)

//progressive scan pattern 
static const uint8_t scan_progressive[16][2] =
{
  {0,0},{1,0},{0,1},{0,2},
  {1,1},{2,0},{3,0},{2,1},
  {1,2},{0,3},{1,3},{2,2},
  {3,1},{3,2},{2,3},{3,3}
};

//! interlaced scan pattern
static const uint8_t scan_interlaced[16][2] =
{
  {0,0},{0,1},{1,0},{0,2},
  {0,3},{1,1},{1,2},{1,3},
  {2,0},{2,1},{2,2},{2,3},
  {3,0},{3,1},{3,2},{3,3}
};


//-------------------------------------------------- sao ------------------------------------------------------
#define MAX_SAO_TRUNCATED_BITDEPTH     10 

#define FULL_NBIT 0 ///< When enabled, compute costs using full sample bitdepth.  When disabled, compute costs as if it is 8-bit source video.
#if FULL_NBIT
# define DISTORTION_PRECISION_ADJUSTMENT(x) 0
#else
# define DISTORTION_PRECISION_ADJUSTMENT(x) (x)
#endif

typedef enum
{
	SAO_TYPE_START_EO =0,
	SAO_TYPE_EO_0 = SAO_TYPE_START_EO,
	SAO_TYPE_EO_90,
	SAO_TYPE_EO_135,
	SAO_TYPE_EO_45,

	SAO_TYPE_START_BO,
	SAO_TYPE_BO = SAO_TYPE_START_BO,

	NUM_SAO_NEW_TYPES
}SAOModeNewTypes;
#define NUM_SAO_EO_TYPES_LOG2 2

typedef enum
{
	SAO_CLASS_EO_FULL_VALLEY = 0,
	SAO_CLASS_EO_HALF_VALLEY = 1,
	SAO_CLASS_EO_PLAIN       = 2,
	SAO_CLASS_EO_HALF_PEAK   = 3,
	SAO_CLASS_EO_FULL_PEAK   = 4,
	NUM_SAO_EO_CLASSES,
}SAOEOClasses;
#define NUM_SAO_EO_TYPES_LOG2 2


typedef enum
{
  SAO_MODE_OFF = 0,
  SAO_MODE_NEW,
  SAO_MODE_MERGE,
  NUM_SAO_MODES
}SAOMode;

typedef enum
{
  SAO_MERGE_LEFT =0,
  SAO_MERGE_ABOVE,
  NUM_SAO_MERGE_TYPES
}SAOModeMergeTypes;

#define NUM_SAO_BO_CLASSES_LOG2  5
typedef enum
{
	//SAO_CLASS_BO_BAND0 = 0,
	//SAO_CLASS_BO_BAND1,
	//SAO_CLASS_BO_BAND2,
	//...
	//SAO_CLASS_BO_BAND31,

	NUM_SAO_BO_CLASSES = (1<<NUM_SAO_BO_CLASSES_LOG2),
}SAOBOClasses;
#define MAX_NUM_SAO_CLASSES  32  //(NUM_SAO_EO_GROUPS > NUM_SAO_BO_GROUPS)?NUM_SAO_EO_GROUPS:NUM_SAO_BO_GROUPS


typedef struct sao_stat_data_t sao_stat_data_t;
struct sao_stat_data_t //data structure for SAO statistics
{
	int64_t diff[MAX_NUM_SAO_CLASSES];
	int64_t count[MAX_NUM_SAO_CLASSES];
};


typedef struct sao_offset sao_offset_t;
struct sao_offset
{
  int modeIdc; //NEW, MERGE, OFF
  int typeIdc; //NEW: EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
  int typeAuxInfo; //BO: starting band index
  int offset[MAX_NUM_SAO_CLASSES];

//  SAOOffset();
//  ~SAOOffset();
//  Void reset();

//  const SAOOffset& operator= (const SAOOffset& src);
};

typedef struct sao_blk_param sao_blk_param_t;
struct sao_blk_param
{
//  SAOBlkParam();
//  ~SAOBlkParam();
//  Void reset();
//  const SAOBlkParam& operator= (const SAOBlkParam& src);
//  SAOOffset& operator[](int compIdx){ return offsetParam[compIdx];}
//private:
  sao_offset_t offsetParam[NUM_PICT_COMPONENTS];
};




#define MAX_TLAYER                  8           // max number of sublayers (temporal layers en el HM)
typedef struct profile_tier_t   profile_tier_t;

struct profile_tier_t
{
  unsigned int		profileSpace;
  unsigned int		tierFlag;
  unsigned int		profileIdc;

  unsigned int		profileCompatibilityFlag[32];

  unsigned int		general_progressive_source_flag;
  unsigned int		general_interlaced_source_flag;
  unsigned int		general_non_packed_constraint_flag;
  unsigned int		general_frame_only_constraint_flag;
  
  unsigned int		levelIdc;
};

typedef struct profile_tier_level_t profile_tier_level_t;

struct profile_tier_level_t
{
  profile_tier_t	generalPTL;
  profile_tier_t	subLayerPTL[6];      // max. value of max_sub_layers_minus1 is 6
  unsigned int		subLayerProfilePresentFlag[6];
  unsigned int		subLayerLevelPresentFlag[6];	
};

typedef struct vps_t vps_t;

struct vps_t
{
	unsigned int video_parameter_set_id;			//u(4)
	unsigned int temporal_id_nesting_flag;			//u(1)
	//reserved_zero									//u(2) 
	//reserved_zero									//u(6)
//	unsigned int	max_sub_layers_minus1;			//u(3)

	profile_tier_level_t *ptl;

	unsigned int sub_layer_ordering_info_present_flag;//u(1)
	//profile and level
	//reserved_zero									//u(12)
	unsigned int max_dec_pic_buffering[MAX_TLAYER];	//ue(v)
	unsigned int max_num_reorder_pics[MAX_TLAYER];	//ue(v)
	unsigned int max_latency_increase[MAX_TLAYER];	//ue(v)

	//unsigned int vps_max_layer_id;				//u(6)
	unsigned int num_layer_sets_minus1;			//ue(v) //vps_max_op_sets_minus1

	unsigned int timing_info_present_flag;		//u(1)

	unsigned int num_hdr_parameters;				//u(3)

};

typedef struct sps_t sps_t;
struct sps_t
{
	Boolean   Valid;                  // indicates the parameter set is valid

	unsigned int video_parameter_set_id;			//u(4)
//	unsigned int sps_max_sub_layers_minus1;         //u(3)
	//reserved_zero									//u(1) 

	profile_tier_level_t *ptl;						

	unsigned int seq_parameter_set_id;				//ue(v)
	unsigned int chroma_format_idc;					//ue(v)
	//separate_colour_plane_flag					//u(1) 
	unsigned int pic_width_in_luma_samples;			//ue(v)
	unsigned int pic_height_in_luma_samples;		//ue(v)
	unsigned int conformance_window_flag;					//u(1)
	unsigned int conf_win_left_offset;				//ue(v)
	unsigned int conf_win_right_offset;				//ue(v)
	unsigned int conf_win_top_offset;				//ue(v)
	unsigned int conf_win_bottom_offset;			//ue(v)
	unsigned int bit_depth_luma_minus8;				//ue(v)
	unsigned int bit_depth_chroma_minus8;			//ue(v)
	unsigned int pcm_enabled_flag;					//u(1)
	unsigned int pcm_bit_depth_luma_minus1;			//u(4)
	unsigned int pcm_bit_depth_chroma_minus1;		//u(4)
	unsigned int log2_max_pic_order_cnt_lsb_minus4;	//ue(v)
	unsigned int max_dec_pic_buffering[MAX_TLAYER];	//ue(v)
	unsigned int max_num_reorder_pics[MAX_TLAYER];	//ue(v)
	unsigned int max_latency_increase[MAX_TLAYER];	//ue(v)
	unsigned int restricted_ref_pic_lists_flag;		//u(1)
	unsigned int lists_modification_present_flag;	//u(1)
	unsigned int log2_min_coding_block_size_minus3; //ue(v)
	unsigned int log2_diff_max_min_coding_block_size; //ue(v)
	unsigned int log2_min_transform_block_size_minus2;//ue(v)
	unsigned int log2_diff_max_min_transform_block_size;//ue(v)
	unsigned int log2_min_pcm_coding_block_size_minus3;//ue(v)
	unsigned int log2_diff_max_min_pcm_coding_block_size;//ue(v)
	unsigned int max_transform_hierarchy_depth_inter;//ue(v)
	unsigned int max_transform_hierarchy_depth_intra;//ue(v)
	unsigned int scaling_list_enabled_flag;			//u(1)
	unsigned int scaling_list_data_present_flag;	//u(1)
	//scaling_list_data()
	unsigned int amp_enabled_flag;					//u(1)	//amp = asymetric motion partitions
	unsigned int sample_adaptive_offset_enabled_flag;//u(1)
	unsigned int pcm_loop_filter_disable_flag;		//u(1)
	unsigned int temporal_id_nesting_flag;			//u(1)
//	unsigned int num_short_term_ref_pic_sets;		//ue(v) - moved to enc_engine
	//hmr_short_term_ref_pic_set(i)								- moved to enc_engine
//	unsigned int long_term_ref_pics_present_flag;	//u(1)	- moved to enc_engine
//	unsigned int num_long_term_ref_pic_sps;			//ue(v)	- moved to enc_engine
//	unsigned int lt_ref_pic_poc_lsb_sps;			//u(v)
	unsigned int used_by_curr_pic_lt_sps_flag;		//u(1)
	unsigned int temporal_mvp_enable_flag;			//u(1)	//mvp = motion vector prediction
	unsigned int strong_intra_smooth_enabled_flag;	//u(1)
	unsigned int vui_parameters_present_flag;		//u(1)
		//vui_parameters()
	//unsigned int sps_extension_flag;				//u(1)
};


typedef struct pps_t pps_t;
struct pps_t
{
//  Boolean   Valid;                  // indicates the parameter set is valid
	unsigned int pic_parameter_set_id;                          // ue(v)
	unsigned int seq_parameter_set_id;                          // ue(v)
	unsigned int dependent_slice_enabled_flag;					// u(1)
	unsigned int output_flag_present_flag;                      // u(3)
	unsigned int num_extra_slice_header_bits;					// u(1)
	unsigned int sign_data_hiding_flag;							// u(1)
	unsigned int cabac_init_present_flag;                       // u(1)
	unsigned int cabac_init_flag;								// u(1)
	unsigned int num_ref_idx_l0_default_active_minus1;          // ue(v)
	unsigned int num_ref_idx_l1_default_active_minus1;          // ue(v)

	unsigned int pic_init_qp_minus26;							// se(v)

	unsigned int constrained_intra_pred_flag;					// u(1)
	unsigned int transform_skip_enabled_flag;					// u(1)
	unsigned int cu_qp_delta_enabled_flag;						// u(1)
	unsigned int diff_cu_qp_delta_depth;						// ue(v)
	unsigned int cb_qp_offset;								// se(v)
	unsigned int cr_qp_offset;								// se(v)

	unsigned int slice_chroma_qp_offsets_present_flag;			// u(1)
	unsigned int weighted_pred_flag;							// u(1)
	unsigned int weighted_bipred_flag;							// u(1)
	unsigned int transquant_bypass_enable_flag;					// u(1)
	unsigned int tiles_enabled_flag;							// u(1)
	unsigned int entropy_coded_sync_enabled_flag;				// u(1)
//	unsigned int entropy_slice_enabled_flag;					// u(1) - Removed

//	unsigned int num_tile_columns_minus1;						// ue(v)
//	unsigned int num_tile_rows_minus1;							// ue(v)
//	unsigned int uniform_spacing_flag;							// u(1)
//	...........................
	unsigned int loop_filter_across_slices_enabled_flag;		// u(1)
	unsigned int deblocking_filter_control_present_flag;		// u(1)
	unsigned int deblocking_filter_override_enabled_flag;		// u(1)
	unsigned int deblocking_filter_disabled_flag;				// u(1)
	unsigned int beta_offset_div2;								// u(1)
	unsigned int tc_offset_div2;								// u(1)
	//.......................
	unsigned int pps_scaling_list_data_present_flag;			// u(1)
	unsigned int lists_modification_present_flag;				// u(1)
	unsigned int log2_parallel_merge_level_minus2;				// se(v)
	unsigned int slice_header_extension_present_flag;			// se(v)
};


typedef struct wnd_t wnd_t;
struct wnd_t
{
	void	*palloc[NUM_PICT_COMPONENTS];//allocated pointer
	void	*pwnd[NUM_PICT_COMPONENTS];//valid data pointer
	int		window_size_x[NUM_PICT_COMPONENTS];
	int		window_size_y[NUM_PICT_COMPONENTS];
	int		data_padding_x[NUM_PICT_COMPONENTS];//left and right padding due to data pading or memory aligment
	int		data_padding_y[NUM_PICT_COMPONENTS];//top and bottom padding due to data pading or memory aligment
	int		data_width[NUM_PICT_COMPONENTS];//wnd data horizontal size
	int		data_height[NUM_PICT_COMPONENTS];//wnd data vertical size
	int		pix_size;
//#ifdef WRITE_REF_FRAMES
//	FILE	*out_file;//for debug porposes
//#endif
};

typedef union temporal_info_t temporal_info_t;
union temporal_info_t
{
	uint64_t	pts;
	uint32_t	poc;
};

typedef struct video_frame_t video_frame_t;
struct video_frame_t
{
	wnd_t			img;
	int				img_type;
	temporal_info_t	temp_info;
	int				is_reference;
};


#define NALU_SET_SIZE	8
typedef struct output_set_t output_set_t;
struct output_set_t
{
	video_frame_t	*frame;
	nalu_t			*nalu_list[NALU_SET_SIZE];
	int				num_nalus;
	int				image_type;
	uint64_t		pts;
};


typedef struct motion_vector_t motion_vector_t;

struct motion_vector_t
{
  int32_t hor_vector;
  int32_t ver_vector;
};


#define AMVP_MAX_NUM_CANDS			2
#define MERGE_MVP_MAX_NUM_CANDS		5
#define MV_MAX_NUM_CANDS_MEM	MERGE_MVP_MAX_NUM_CANDS//3

typedef struct mv_candiate_list_t mv_candiate_list_t;
struct mv_candiate_list_t
{
	int	num_mv_candidates;
	motion_vector_t	mv_candidates[MV_MAX_NUM_CANDS_MEM];
};

typedef struct cu_partition_info_t cu_partition_info_t;
struct cu_partition_info_t
{
	uint16_t list_index;
	uint16_t depth;
	uint16_t abs_index;
	uint16_t size;
	uint16_t size_chroma;
	uint16_t x_position,x_position_chroma;
	uint16_t y_position,y_position_chroma;
	uint16_t num_part_in_cu;//tama�o en particiones minimas

	uint16_t raster_index;
	uint16_t left_neighbour;
	uint16_t top_neighbour; 
	uint16_t left_bottom_neighbour;
	uint16_t top_right_neighbour; 
	uint16_t is_tl_inside_frame, is_b_inside_frame, is_r_inside_frame;
	uint16_t abs_index_left_partition;
	uint16_t abs_index_left_bottom_partition;
	uint16_t abs_index_top_partition;
	uint16_t abs_index_top_right_partition;
	uint16_t abs_index_top_left_partition;

	cu_partition_info_t	*parent;//pointer to parent partition
	cu_partition_info_t	*children[4];//pointers to child partitions
	
	uint32_t qp;
//	uint16_t mode;
//	uint16_t mode_chroma;
	//intra
	uint32_t sum;
//	uint distortion_chroma, cost_chroma;
	uint32_t sad;
	uint32_t distortion, cost;
	uint32_t variance, variance_luma, variance_chroma;
	uint32_t recursive_split;
	//inter prediction. Trying to avoid buffer consolidation
//	uint inter_distortion, inter_distortion_chroma;
//	uint inter_cost, inter_cost_chroma;
	int prediction_mode;
	int merge_flag, merge_idx, skipped;
	int intra_cbf[NUM_PICT_COMPONENTS], intra_tr_idx, intra_mode[NUM_PICT_COMPONENTS];
	int inter_cbf[NUM_PICT_COMPONENTS], inter_tr_idx;
	motion_vector_t	inter_mv[2];
	motion_vector_t	subpix_mv[2];
	motion_vector_t	best_dif_mv[2];
	int 	best_candidate_idx[2];
	int		inter_ref_index[2];
};

typedef struct ctu_info_t ctu_info_t ;
struct ctu_info_t 
{
	int				ctu_number;
	int				x[3],y[3];//global coordinates in frame
	int				size;
	int				num_part_in_ctu;
	int				last_valid_partition;
	uint32_t		distortion;
	cu_partition_info_t	*partition_list;

	uint8_t			*cbf[NUM_PICT_COMPONENTS];//[MAX_NUM_PARTITIONS];
	uint8_t			*intra_mode[NUM_PICT_COMPONENTS-1];//[MAX_NUM_PARTITIONS];
	uint8_t			*inter_mode;//[MAX_NUM_PARTITIONS];
	uint8_t			*tr_idx;//[MAX_NUM_PARTITIONS];
	uint8_t			*pred_depth;//[MAX_NUM_PARTITIONS];
	uint8_t			*part_size_type;//[MAX_NUM_PARTITIONS];
	uint8_t			*pred_mode;//[MAX_NUM_PARTITIONS];//intra or inter
	uint8_t			*skipped;//[MAX_NUM_PARTITIONS];
	uint8_t			*merge;
	uint8_t			*merge_idx;
	uint8_t			*qp;
	wnd_t			*coeff_wnd;

	//inter
	motion_vector_t		*mv_ref[2];
	int8_t				*mv_ref_idx[2];
	motion_vector_t		*mv_diff[2];
	uint8_t				*mv_diff_ref_idx[2];

//	uint8_t				*ref_idx1;

	ctu_info_t		*ctu_left;
	ctu_info_t		*ctu_left_bottom;
	ctu_info_t		*ctu_top;
	ctu_info_t		*ctu_top_right;
	ctu_info_t		*ctu_top_left;
	int				top;
	int				left;
	
	//sao
//	sao_stat_data_t (*stat_data)[NUM_PICT_COMPONENTS][NUM_SAO_NEW_TYPES];//to save memory this can be allocated in a wpp thread basis when processing is distributed (COMPUTE_AS_HM undefined)
	sao_stat_data_t stat_data[NUM_PICT_COMPONENTS][NUM_SAO_NEW_TYPES];//to save memory this can be allocated in a wpp thread basis when processing is distributed (COMPUTE_AS_HM undefined)
	sao_blk_param_t recon_params;
	sao_blk_param_t coded_params;
	//quant
//	int				/*qp, */qp_chroma;/*, prev_qp, prev_dqp*/
//    int				per;//, per_chroma;
//    int				rem;//, rem_chroma;
//    int				qpbits;
//	uint			variance;
};




#define FAST_BIT_EST	1

//TEncBinCABAC
typedef struct binary_model_t binary_model_t;
struct binary_model_t
{
	uint	m_uiLow;
	uint	m_uiRange;
	uint	m_bufferedByte;
	int		m_numBufferedBytes;
	uint	m_bitsLeft;
	uint	m_uiBinsCoded;
	int		m_binCountIncrement;
#if FAST_BIT_EST
	uint64	m_fracBits;
#endif
};



//ContextModel
typedef struct context_model_t context_model_t;
struct context_model_t
{
	uint		num_bins_coded;
	uint8_t		state;
};

//ContextModel3DBuffer
typedef struct context_model_buff_t context_model_buff_t;
struct context_model_buff_t
{
	context_model_t *ctx;
	int size_x;
	int size_y;
	int size_xy;
	const uint8_t *ref_ctx_model;
};

//estos contextos en el HM estan dentro de TEncSbac
typedef struct entropy_model_t entropy_model_t;
struct entropy_model_t
{
	context_model_buff_t	cu_split_flag_model;
	context_model_buff_t	cu_skip_flag_model;
	context_model_buff_t	cu_merge_flag_model;
	context_model_buff_t	cu_merge_idx_model;
	context_model_buff_t	cu_part_size_model;
	context_model_buff_t	cu_pred_mode_flag_model;
	context_model_buff_t	cu_intra_pred_model;
	context_model_buff_t	cu_chroma_pred_model;
	context_model_buff_t	cu_delta_qp_model;
	context_model_buff_t	cu_inter_dir_model;
	context_model_buff_t	cu_ref_pic_model;
	context_model_buff_t	cu_mvd_model;
	context_model_buff_t	cu_qt_cbf_model;
	context_model_buff_t	cu_trans_subdiv_flag_model;
	context_model_buff_t	cu_qt_root_cbf_model;
	context_model_buff_t	cu_sig_coeff_group_model;
	context_model_buff_t	cu_sig_model;
	context_model_buff_t	cu_ctx_last_x_model;
	context_model_buff_t	cu_ctx_last_y_model;
	context_model_buff_t	cu_one_model;
	context_model_buff_t	cu_abs_model;
	context_model_buff_t	cu_mvp_idx_model;
	context_model_buff_t	cu_amp_model;
	context_model_buff_t	sao_merge_model;
	context_model_buff_t	sao_type_model;
	context_model_buff_t	transform_skip_model;
	context_model_buff_t	transquant_bypass_flag_model;
};


typedef enum 
{
	EE_ENCODER,
	EE_COUNTER,
	EE_INVALID
}enc_env_enum;

//equivale a TEncSbac
typedef struct enc_env_t enc_env_t;
struct enc_env_t
{
	context_model_t	*contexts;
	binary_model_t	*b_ctx;
	entropy_model_t	*e_ctx;
	bitstream_t		*bs;
	enc_env_enum		type;
//	function pointers
	void (*ee_reset_bits)(binary_model_t* bm);
	void (*ee_start)(binary_model_t* bm);
	uint (*ee_bitcnt)(bitstream_t *bs, binary_model_t* bm);
	void (*ee_encode_bin)( enc_env_t* ee, context_model_t *cm, uint binValue);
	void (*ee_encode_bin_TRM)( enc_env_t* ee, uint binValue);
	void (*ee_encode_bins_EP)( enc_env_t* ee, uint binValues, int numBins );
	void (*ee_encode_bin_EP)( enc_env_t* ee, uint binValue);
	void (*ee_finish)( enc_env_t* ee);
};

typedef struct rate_distortion_t rate_distortion_t;
struct rate_distortion_t
{
	double    lambda;
	double    sqrt_lambda;
	uint      lambda_SAD;
	uint      lambda_SSE;
	double    frame_lambda;
};


typedef struct rate_control_t rate_control_t;
struct rate_control_t
{
	double	vbv_size;
	double	average_pict_size;
	double  average_bits_per_ctu;
	double	vbv_fullness;
	double	target_pict_size;
	double	target_bits_per_ctu;
	double  acc_rate;
	double  acc_avg;
	int		extra_bits;
//	int		acc_qp;
//	double	consumed_bitrate;
//	int		consumed_ctus;
};


#define MAX_NUM_REF_PICS            16						//max. number of pictures used for reference
#define MAX_NUM_REF                 16						//max. number of entries in picture reference list
#define MAX_NUM_REF_MASK            (MAX_NUM_REF-1)         //max. number of entries in picture reference list

typedef struct ref_pic_set_t ref_pic_set_t;
struct ref_pic_set_t
{
	int inter_ref_pic_set_prediction_flag;
	//..................
	int num_pics;
	int num_negative_pics;
	int num_positive_pics;
	int delta_poc_s0[MAX_NUM_REF];
	int used_by_curr_pic_S0_flag[MAX_NUM_REF];
};

typedef struct slice_t slice_t;
struct slice_t
{
	uint32_t slice_index; //indice en la lista de slices
	uint32_t slice_type;
	uint32_t nalu_type;
	uint32_t first_cu_address;//address of first coding tree block in the slice
	uint32_t curr_cu_address;//address of current coding tree block in the slice
	uint32_t last_cu_address;//m_uiDependentSliceCurEndCUAddr - address of current coding tree block in the slice partition units (256 per CU)
	uint32_t is_dependent_slice;//
	uint32_t slice_temporal_layer_non_reference_flag;//
	uint32_t slice_temporal_mvp_enable_flag;
	uint32_t deblocking_filter_disabled_flag;
	uint32_t sao_luma_flag;
	uint32_t sao_chroma_flag;
	uint32_t slice_loop_filter_across_slices_enabled_flag;
	uint32_t slice_beta_offset_div2;
	uint32_t slice_tc_offset_div2;
	uint32_t max_num_merge_candidates;

	sps_t		*sps;
	pps_t		*pps;
	
	int qp;
	uint32_t poc;
	uint32_t depth;
	uint32_t sublayer;//TLayer
	uint32_t referenced;
	uint32_t num_ref_idx[2];
	uint32_t ref_pic_set_index;
	ref_pic_set_t	*ref_pic_set;
	video_frame_t	*ref_pic_list[2][MAX_NUM_REF];
	int				ref_pic_list_cnt[2];
};

typedef struct picture_t picture_t;
struct picture_t
{
	slice_t	slice;
	video_frame_t	*img2encode;
};



typedef struct henc_thread_t henc_thread_t;
typedef struct hvenc_engine_t hvenc_engine_t;
typedef struct hvenc_enc_t hvenc_enc_t;

typedef struct low_level_funcs_t low_level_funcs_t;
struct low_level_funcs_t
{
	uint32_t (*sad)(uint8_t * src, uint32_t src_stride, int16_t * pred, uint32_t pred_stride, int size);
	uint32_t (*ssd)(uint8_t * src, uint32_t src_stride, int16_t * pred, uint32_t pred_stride, int size);
	uint32_t (*ssd16b)(int16_t * src, uint32_t src_stride, int16_t * pred, uint32_t pred_stride, int size);
	void (*predict)(uint8_t * orig, int orig_stride, int16_t* pred, int pred_stride, int16_t * residual, int residual_stride, int size);
	void (*reconst)(int16_t* pred, int pred_stride, int16_t * residual, int residual_stride, int16_t* decoded, int decoded_stride, int size);
	uint32_t (*modified_variance)(uint8_t *p, int size, int stride, int modif);

	void (*create_intra_planar_prediction)(henc_thread_t* et, int16_t *prediction, int pred_stride, int16_t  *adi_pred_buff, int adi_size, int cu_size, int cu_size_shift);
	void (*create_intra_angular_prediction)(henc_thread_t* et, ctu_info_t* ctu, int16_t *prediction, int pred_stride, int16_t  *adi_pred_buff, int adi_size, int cu_size, int cu_mode, int is_luma);

	void (*interpolate_luma_m_compensation)(int16_t *reference_buff, int reference_buff_stride, int16_t *pred_buff, int pred_buff_stride, int fraction, int width, int height, int is_vertical, int is_first, int is_last);
	void (*interpolate_chroma_m_compensation)(int16_t *reference_buff, int reference_buff_stride, int16_t *pred_buff, int pred_buff_stride, int fraction, int width, int height, int is_vertical, int is_first, int is_last);
	void (*interpolate_luma_m_estimation)(int16_t *reference_buff, int reference_buff_stride, int16_t *pred_buff, int pred_buff_stride, int fraction, int width, int height, int is_vertical, int is_first, int is_last);
//	void (*interpolate_chroma_m_estimation)(int16_t *reference_buff, int reference_buff_stride, int16_t *pred_buff, int pred_buff_stride, int fraction, int width, int height, int is_vertical, int is_first, int is_last);

	void (*quant)(henc_thread_t* et, int16_t* src, int16_t* dst, int scan_mode, int depth, int comp, int cu_mode, int is_intra, int *ac_sum, int cu_size, int per, int rem);
	void (*inv_quant)(henc_thread_t* et, short * src, short * dst, int depth, int comp, int is_intra, int cu_size, int per, int rem);

	void (*transform)(int bit_depth, int16_t *block,int16_t *coeff, int block_size, int iWidth, int iHeight, int width_shift, int height_shift, uint16_t uiMode, int16_t *aux);
	void (*itransform)(int bit_depth, int16_t *block,int16_t *coeff, int block_size, int iWidth, int iHeight, unsigned int uiMode, int16_t *aux);

	void (*get_sao_stats)(henc_thread_t *wpp_thread, slice_t *currslice, ctu_info_t* ctu, sao_stat_data_t stats[][NUM_SAO_NEW_TYPES]);
};





#define NUM_QUANT_WNDS			(MAX_PARTITION_DEPTH+2)//consolidation buffer + 5 depths + 1 extra depth for non 2Nx2N computation in inter prediction
#define NUM_DECODED_WNDS		(MAX_PARTITION_DEPTH+2)//consolidation buffer + 5 depths + 1 extra depth for non 2Nx2N computation in inter prediction
#define NUM_CBF_BUFFS			MAX_PARTITION_DEPTH

struct henc_thread_t
{
	hvenc_engine_t	*enc_engine;
	uint		index;
	int			wfpp_enable;
	int			wfpp_num_threads;
	hmr_sem_t	synchro_sem[2];// 0 for intra_frame synchronization, 1 for inter frame synchronization
	hmr_sem_ptr	synchro_signal[2];// 0 for intra_frame synchronization, 1 for inter frame synchronization
	hmr_sem_ptr	synchro_wait[2];// 0 for intra_frame synchronization, 1 for inter frame synchronization
	int			num_wait_sem;
	int			dbg_sem_post_cnt;
	bitstream_t	*bs;

	//header info
	vps_t	*vps;
	sps_t	*sps;
	pps_t	*pps;

	//Encoder Cfg	
	//Encoding layer
	int				pict_width[3], pict_height[3];
	int				pict_width_in_ctu, pict_height_in_ctu;
	int				pict_total_ctu;
	int				ctu_width[3], ctu_height[3];
//	int				ctu_group_size;

	//cfg
	int				max_cu_size;
	int				max_cu_size_shift;//log2 of max cu size
	int				max_cu_size_shift_chroma;
	int				max_intra_tr_depth;
	int				max_inter_tr_depth;
	int				max_pred_partition_depth;//, max_inter_pred_depth;//max depth for prediction
	int				motion_estimation_precision;

	int				num_partitions_in_cu;
	int				num_partitions_in_cu_shift;
	int				mincu_mintr_shift_diff;//log2		//g_uiAddCUDepth
	int				max_cu_depth;							//m_uiMaxCUDepth
	int				min_cu_size;
	int				min_cu_size_shift;//log2
	int				min_tu_size_shift;//log2
	int				max_tu_size_shift;//log2

	int				profile;
	int				bit_depth;
	int				rd_mode;
	int				performance_mode;
	uint			num_intra_partitions;
	uint			num_total_partitions;
	uint			num_total_ctus;

	int				*partition_depth_start;//start of depths in the partition_info list
//	cu_partition_info_t	*partition_info;//recursive structure list to store the state of the recursive computing stages
	//current processing state and buffers
	int				cu_current, cu_next;
	int				cu_current_x, cu_current_y;

	wnd_t			curr_mbs_wnd;									//original MBs to be coded
	wnd_t			prediction_wnd;									//prediction applied to original MBs
	wnd_t			residual_wnd;									//residual after substracting prediction
	wnd_t			residual_dec_wnd;								//decoded residual. output of inverse transform
	wnd_t			transform_quant_wnd_[NUM_QUANT_WNDS];			//windows to be used with pointers 
	wnd_t			*transform_quant_wnd[NUM_QUANT_WNDS];			//for transform coefficients and quantification 
	wnd_t			itransform_iquant_wnd;							//for itransform coefficients and iquantification
	wnd_t			decoded_mbs_wnd_[NUM_DECODED_WNDS];				//windows to be used with pointers
	wnd_t			*decoded_mbs_wnd[NUM_DECODED_WNDS];

	//deblock filter
//	cu_partition_info_t* deblock_partition_info;//recursive structure list to store the state of the recursive computing stages
	uint8_t			*deblock_edge_filter[2];
	uint8_t			*deblock_filter_strength_bs[2];

	//sao
	int16_t			*sao_sign_line_buff1;//m_signLineBuf1;
	int16_t			*sao_sign_line_buff2; //m_signLineBuf2;

	//quarter precission buffers
	wnd_t			filtered_block_wnd[4][4];
	wnd_t			filtered_block_temp_wnd[4];

	//intra predicition
	int16_t				(*adi_pred_buff);//this buffer holds the left column and top row for intra pred (bottom2top and left2right)
	int16_t				(*adi_filtered_pred_buff);//this buffer holds the left column and top row for intra pred (bottom2top and left2right)
	int16_t				(*top_pred_buff);//intermediate buffer to calculate intra prediction samples
	int16_t				(*left_pred_buff);//intermediate buffer to calculate intra prediction samples
	int16_t				(*bottom_pred_buff);//intermediate buffer to calculate intra prediction samples
	int16_t				(*right_pred_buff);//intermediate buffer to calculate intra prediction samples
	int					adi_size;//size of adi buffer 
	int16_t				(*pred_aux_buff);
	int					pred_aux_buff_size;//size of aux buffer
	int16_t				(*aux_buff);
	uint8_t				(*cabac_aux_buff);
	int					cabac_aux_buff_size;
//	ctu_info_t			*curr_ctu_group_info;	//this is supposed to be a small window matching the processing grain
	uint8_t				*cbf_buffs[NUM_PICT_COMPONENTS][MAX_PARTITION_DEPTH];
	uint8_t				*cbf_buffs_chroma[NUM_PICT_COMPONENTS];//processing aux buffers
	uint8_t				*intra_mode_buffs[NUM_PICT_COMPONENTS][MAX_PARTITION_DEPTH];
	uint8_t				*tr_idx_buffs[MAX_PARTITION_DEPTH];

	//inter
	mv_candiate_list_t	amvp_candidates[2];
	mv_candiate_list_t	merge_mvp_candidates[2];
	mv_candiate_list_t	mv_search_candidates;//non normative candidate list for motion_search

	//rd
	ctu_info_t			*ctu_rd;//[MAX_MB_GROUP_SIZE];

	enc_env_t			*ee;//encoding enviroment of the processing element
	enc_env_t			*ec;//encoding counter  of the processing element
	context_model_t		*aux_contexts;
	binary_model_t		aux_bm;
	//rate distortion
	rate_distortion_t	rd;
	uint32_t			acc_dist;

	//rate control
	uint				num_encoded_ctus;//num CABAC encoded ctus
	uint				num_bits;
	uint				target_pict_size;
	int					acc_qp;
	int					write_qp_flag;
	int					curr_ref_qp;//for qp modification when cbf=0
	int					found_zero_cbf;//for qp modification when cbf=0
	low_level_funcs_t	*funcs;
};


#define MAX_NUM_ENCODER_ENGINES				8
#define STREAMS_PER_ENGINE					2
#define MAX_NUM_THREADS						32
#define NUM_INPUT_FRAMES(num_engines)		(num_engines)//((STREAMS_PER_ENGINE-1)*num_engines)
#define NUM_OUTPUT_NALUS(num_engines)		(2*NUM_INPUT_FRAMES(num_engines))
//#define NUM_OUTPUT_NALUS_MASK	(NUM_OUTPUT_NALUS-1)
struct hvenc_engine_t
{
	int				index;
	int				num_encoded_frames;
	int				num_encoded_frames_in_engine;
	hvenc_enc_t		*hvenc;//parent encoder layer
	henc_thread_t	*thread[MAX_NUM_THREADS];//*encoders_list;
	hmr_thread_t	hthreads[MAX_NUM_THREADS];
	int				dbg_num_posts[MAX_NUM_THREADS];

	nalu_t			slice_nalu_list[STREAMS_PER_ENGINE];//slice
	nalu_t			*slice_nalu;//slice
	bitstream_t		slice_bs;//slice information previous to nalu_ebsp conversion
	bitstream_t		*aux_bs;//list of bitstreams for coef wfpp encoding
//	bitstream_t		*bc_bs;//list of bitstreams for coef wfpp encoding
	int				num_sub_streams;
	uint			*sub_streams_entry_point_list;

	//Encoder Cfg	
	//Encoding layer
	int				intra_period;
	uint32_t		last_intra;
	uint32_t		last_gop_reinit;
	int				gop_size;
	int				gop_reinit_on_scene_change;
	int				num_b;
	uint			conformance_mode;
	uint			pad_left, pad_right;
	uint			pad_top, pad_bottom;
	int				pict_width[3], pict_height[3];
	int				pict_width_in_ctu, pict_height_in_ctu;
	int				pict_total_ctu;
	int				ctu_width[3], ctu_height[3];

	//statistics for scene change detection
	double			avg_dist;
	int				is_scene_change;
	//cfg
	float			frame_rate;
	int				max_cu_size;
	int				max_cu_size_shift;//log2 of cu max size
	int				max_cu_size_shift_chroma;//log2 of cu chroma max size
	int				max_intra_tr_depth;
	int				max_inter_tr_depth;
	int				max_pred_partition_depth;//, max_inter_pred_depth;//max depth for prediction
	int				motion_estimation_precision;
	int				num_merge_mvp_candidates;
	int				wfpp_enable;
	int				wfpp_num_threads;

	//
	int				num_partitions_in_cu;
	int				num_partitions_in_cu_shift;
	int				mincu_mintr_shift_diff;//log2		//g_uiAddCUDepth
	int				max_cu_depth;							//m_uiMaxCUDepth
	int				min_cu_size;
	int				min_cu_size_shift;//log2
	int				min_tu_size_shift;//log2
	int				max_tu_size_shift;//log2

	int				partition_depth_start[MAX_PARTITION_DEPTH];//start of depths in the partition_info list

	int				bit_depth;
	int				max_sublayers;
	int				max_layers;

	//sao cfg
	int 			calculate_preblock_stats;
	int				slice_enabled[NUM_PICT_COMPONENTS];
	//current picture_t Config
	picture_t		current_pict;
	video_frame_t	*curr_reference_frame;
	wnd_t			sao_aux_wnd;

	int				last_poc, last_idr;//, num_pictures;
	int				num_ref_lists;
	int				num_refs_idx_active_list[2];
	int				num_ref_frames;

	//intra predicition
	ctu_info_t			*ctu_info;//[MAX_MB_GROUP_SIZE];
	int					performance_mode;
	int					performance_fast_skip_loop;//fast skip loops for faster performance
	int					performance_min_depth;//starting depth for faster performance
	int					rd_mode;
	int					bitrate_mode;
	double				bitrate;
	double				vbv_size;
	double				vbv_init;
	int					qp_min;
	int					qp_depth;//granularity of qp diff
	int					pict_qp, chroma_qp_offset;//for fixed qp mode or initial qp in cbr or vbr

	//scan tables	 
	//-------these are for abs_index partitions---------------------
	uint16_t			*abs2raster_table; //g_auiZscanToRaster en HM
	uint16_t			*raster2abs_table; //g_auiRasterToZscan en HM
	//--------------------------------------------------------------
	const uint16_t		*ang_table;//for angular intra prediction    
	const uint16_t		*inv_ang_table;//for angular intra prediction

	//arithmetic coding
	int					num_ee;
	enc_env_t			**ee_list;//encoding enviroment list 
	int					num_ec;
	enc_env_t			*ec_list;//enconding context list

	//rate distortion
	rate_distortion_t	rd;
	double				sao_lambdas[NUM_PICT_COMPONENTS];
	//rate control
	rate_control_t		rc;

	//input and output
	hmr_sem_t		input_sem;
	hmr_sem_ptr		input_signal;
	hmr_sem_ptr		input_wait;

	hmr_sem_t		output_sem;
	hmr_sem_ptr		output_signal;
	hmr_sem_ptr		output_wait;

#ifdef COMPUTE_METRICS
	double			current_psnr[3];
	double			accumulated_psnr[3];
//	FILE			*f_psnr;
#endif
//	FILE			*debug_file;
	int				sao_debug_mode[3];
	int				sao_debug_type[5];
};


struct hvenc_enc_t
{
	hvenc_engine_t	*encoder_engines[MAX_NUM_ENCODER_ENGINES];
	hmr_thread_t	encoder_mod_thread[MAX_NUM_ENCODER_ENGINES];
	int				num_encoder_engines;
//	hmr_mutex		mutex_start_frame; 

	int				run;
	int				num_encoded_frames;
	int				last_idr, last_intra, last_gop_reinit;
	double			avg_dist;
	uint32_t		poc;
	int				max_sublayers, max_layers;

	int				profile;
	int				intra_period;
	int				num_ref_frames;
	int				gop_size;
	int				num_b;
	profile_tier_level_t	ptl;
	int				pict_height[3], pict_width[3];
	int				ctu_width[3], ctu_height[3];
	int				bit_depth;

	//header info
	vps_t		vps;
	sps_t		sps;
	pps_t		pps;

	//nalus
	nalu_t		vps_nalu;
	nalu_t		sps_nalu;
	nalu_t		pps_nalu;

	bitstream_t	aux_bs;//list of bitstreams for coef wfpp encoding


	//reference pictures
	video_frame_t		ref_wnds[MAX_NUM_REF*2];
//	int					ref_pic_set_index;//moved to slice_t
	int					num_short_term_ref_pic_sets;
	ref_pic_set_t		*ref_pic_set_list;//[MAX_REF_PIC_SETS];
	int					num_long_term_ref_pic_sets;

	video_frame_t		*reference_picture_buffer[MAX_NUM_REF];//reference windows being used
	int					reference_list_index;

	//input and output
	video_frame_t		input_frames[NUM_INPUT_FRAMES(MAX_NUM_ENCODER_ENGINES)];
	void				*input_hmr_container;
	output_set_t		output_sets[NUM_OUTPUT_NALUS(MAX_NUM_ENCODER_ENGINES)];
	void				*output_hmr_container;
	
	//for reconstructed img and reference frames
	void				*cont_empty_reference_wnds;

	//scan tables	 
	//-------these are for abs_index partitions---------------------
	uint16_t			*abs2raster_table; //g_auiZscanToRaster en HM
	uint16_t			*raster2abs_table; //g_auiRasterToZscan en HM
	//--------------------------------------------------------------
	const uint16_t		*ang_table;//for angular intra prediction    
	const uint16_t		*inv_ang_table;//for angular intra prediction
	uint32_t			*scan_pyramid[NUM_SCAN_MODES][MAX_CU_DEPTHS];//[4][7]
	int32_t				*quant_pyramid[NUM_SCALING_MODES][NUM_SCALING_LISTS][NUM_SCALING_REM_LISTS];//[4][6][6]
	int32_t				*dequant_pyramid[NUM_SCALING_MODES][NUM_SCALING_LISTS][NUM_SCALING_REM_LISTS];//[4][6][6]
	double				*scaling_error_pyramid[NUM_SCALING_MODES][NUM_SCALING_LISTS][NUM_SCALING_REM_LISTS];//[4][6][6]//quizas esto lo tendriamos que pasar a int. tiene valores muy bajos

	rate_control_t		rc;

	low_level_funcs_t	funcs;
};


#endif  /* __HOMER_HEVC_PRIVATE_H__*/
