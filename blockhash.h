/* 
 * File:   blockhash.h
 * Author: dudae
 *
 * Created on Utorok, 2015, júna 30, 7:56
 */

#ifndef BLOCKHASH_H
#define	BLOCKHASH_H

#include <stdint.h>
#include <string>
#include <vector>

namespace blockhash {
    
    std::vector<uint64_t> compute(const std::string& filename, int size = 256);
    
    std::vector<uint64_t> compute(const std::vector<uint8_t>& data, int size = 256);
}

#endif	/* BLOCKHASH_H */

