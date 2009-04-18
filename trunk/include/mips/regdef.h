/*

Module Name:

    regdef.h

Abstract:

    MIPS register symbolic name definitions.

Author:

    Michal Strehovsky

Revision History:

*/


#ifndef __REGDEF_H__
#define __REGDEF_H__

/* CP0 registers */
#define index       $0
#define random      $1
#define entrylo0    $2
#define entrylo1    $3
#define pagemask    $5
#define entryhi     $10

#define status      $12
#define cause       $13
#define epc         $14

#define count       $9
#define compare     $11

/* General purpose registers */
#define zero        $0

#define AT  $1

#define v0  $2
#define v1  $3

#define a0  $4
#define a1  $5
#define a2  $6
#define a3  $7

#define t0  $8
#define t1  $9
#define t2  $10
#define t3  $11
#define t4  $12
#define t5  $13
#define t6  $14
#define t7  $15
#define t8  $24
#define t9  $25

#define s0  $16
#define s1  $17
#define s2  $18
#define s3  $19
#define s4  $20
#define s5  $21
#define s6  $22
#define s7  $23
#define s8  $30

#define k0  $26
#define k1  $27

#define gp  $28
#define sp  $29
#define fp  $30
#define ra  $31


#endif
