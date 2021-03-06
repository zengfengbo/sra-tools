/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#ifndef _h_vdb_dump_num_gen_
#define _h_vdb_dump_num_gen_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/vector.h>

/********************************************************************************

                   A NUMBER GENERATOR

input : string, for instance "3,6,8,12,44-49"
ouptut: sequence of integers, for instance 3,6,8,12,44,45,46,47,48,49

********************************************************************************/
typedef struct num_gen_node
{
    uint64_t start;
    uint64_t count;
} num_gen_node;
typedef num_gen_node* p_num_gen_node;


typedef struct num_gen
{
    Vector nodes;
    uint32_t node_count;
    uint64_t curr_node;
    uint64_t curr_node_sub_pos;
} num_gen;

rc_t vdn_make( num_gen** generator );
rc_t vdn_destroy( num_gen* generator );

uint32_t vdn_parse( num_gen* generator, const char* src );

bool vdn_set_range( num_gen* generator,
                    const int64_t first, const uint64_t count );
bool vdn_check_range( num_gen* generator,
                      const int64_t first, const uint64_t count );

bool vdn_start( num_gen* generator );
bool vdn_next( num_gen* generator, uint64_t* value );
bool vdn_range_defined( num_gen* generator );

uint64_t vdn_max( num_gen* generator );

#ifdef __cplusplus
}
#endif

#endif
