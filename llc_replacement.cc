#include "cache.h"

// initialize replacement state
void CACHE::llc_initialize_replacement()
{
}

// find replacement victim
//Change
pair<uint32_t, uint32_t> CACHE::llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    pair<uint32_t, int32_t> victim;      // <set, way>
    pair<uint32_t, uint32_t> victim_lru; // <set, way> for lru_victim oh hot set

    if (warmup_complete[cpu]) // after warmup
    {
        uint16_t set_type = cache_organiser.get_set_type(set); // get set type
        uint32_t way;

        if (set_type == COLD || set_type == VERY_COLD) // for cold and very cold sets
        {
            victim.first = set;

            // fill invalid line first
            for (way = 0; way < NUM_WAY; way++)
            {
                if (block[set][way].foreign == false && block[set][way].valid == false)
                {
                    DP(if (warmup_complete[cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id << " invalid set: " << set << " way: " << way;
                    cout << hex << " address: " << (full_addr>>LOG2_BLOCK_SIZE) << " victim address: " << block[set][way].address << " data: " << block[set][way].data;
                    cout << dec << " lru: " << block[set][way].lru << endl; });

                    break;
                }
            }
            // LRU victim
            if (way == NUM_WAY)
            {
                // calculate max lru
                uint32_t max_lru = (set_type == VERY_COLD) ? NUM_WAY / 2 - 1 : (NUM_WAY * 3) / 4 - 1;

                for (way = 0; way < NUM_WAY; way++)
                {
                    if (block[set][way].foreign == false && block[set][way].lru == max_lru)
                    {

                        DP(if (warmup_complete[cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id << " replace set: " << set << " way: " << way;
                        cout << hex << " address: " << (full_addr>>LOG2_BLOCK_SIZE) << " victim address: " << block[set][way].address << " data: " << block[set][way].data;
                        cout << dec << " lru: " << block[set][way].lru << endl; });

                        break;
                    }
                }
            }

            // Error handling
            if (way == NUM_WAY)
            {
                cerr << "[" << NAME << "] " << __func__ << " no victim! set: " << set << endl;
                assert(0);
            }

            victim.second = way;
            return victim;
        }
        else // for hot and very hot sets
        {
            victim.first = set;

            // calculate max lru
            uint32_t max_lru = (set_type == VERY_HOT) ? (NUM_WAY * 3) / 2 - 1 : (NUM_WAY * 5) / 4 - 1;

            // fill own bocks first if any block is empty
            for (way = 0; way < NUM_WAY; way++)
            {
                if (block[set][way].foreign == false && block[set][way].valid == false)
                {
                    DP(if (warmup_complete[cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id << " invalid set: " << set << " way: " << way;
                    cout << hex << " address: " << (full_addr>>LOG2_BLOCK_SIZE) << " victim address: " << block[set][way].address << " data: " << block[set][way].data;
                    cout << dec << " lru: " << block[set][way].lru << endl; });

                    break;
                }

                // finding lru victim
                if (block[set][way].foreign == false && block[set][way].lru == max_lru)
                {
                    victim_lru.first = set;
                    victim_lru.second = way;
                }
            }
            // fill helper set if any block is empty
            if (way == NUM_WAY)
            {
                // finding helper set
                int32_t helper = cache_organiser.get_helper_set(set);

                // if helper found
                if (helper != -1)
                {
                    victim.first = helper;
                    for (way = 0; way < NUM_WAY; way++)
                    {
                        if (block[helper][way].foreign && block[helper][way].valid == false)
                        {
                            DP(if (warmup_complete[cpu]) {
                            cout << "[" << NAME << "] " << __func__ << " instr_id: " << instr_id << " invalid set: " << set << " way: " << way;
                            cout << hex << " address: " << (full_addr>>LOG2_BLOCK_SIZE) << " victim address: " << block[set][way].address << " data: " << block[set][way].data;
                            cout << dec << " lru: " << block[set][way].lru << endl; });

                            break;
                        }

                        // finding lru victim
                        if (block[helper][way].foreign && block[helper][way].lru == max_lru)
                        {
                            victim_lru.first = helper;
                            victim_lru.second = way;
                        }
                    }
                }
                else
                {
                    // Error handling
                    cerr << "Error at llc_update_replacement_state in getting helper set" << endl;
                    assert(0);
                }
            }

            // if no invalid block found
            if (way == NUM_WAY)
            {
                return victim_lru;
            }

            victim.second = way;
            return victim;
        }
    }
    else
    {
        victim.first = set;
        victim.second = lru_victim(cpu, instr_id, set, current_set, ip, full_addr, type);
        return victim;
    }
}
//Change
// called on every cache hit and cache fill
void CACHE::llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    string TYPE_NAME;
    if (type == LOAD)
        TYPE_NAME = "LOAD";
    else if (type == RFO)
        TYPE_NAME = "RFO";
    else if (type == PREFETCH)
        TYPE_NAME = "PF";
    else if (type == WRITEBACK)
        TYPE_NAME = "WB";
    else
        assert(0);

    if (hit)
        TYPE_NAME += "_HIT";
    else
        TYPE_NAME += "_MISS";

    if ((type == WRITEBACK) && ip)
        assert(0);

    // uncomment this line to see the LLC accesses
    // cout << "CPU: " << cpu << "  LLC " << setw(9) << TYPE_NAME << " set: " << setw(5) << set << " way: " << setw(2) << way;
    // cout << hex << " paddr: " << setw(12) << paddr << " ip: " << setw(8) << ip << " victim_addr: " << victim_addr << dec << endl;

    // baseline LRU
    if (hit && (type == WRITEBACK)) // writeback hit does not update LRU state
        return;
//Change
    if (warmup_complete[cpu]) // after warmup
    {
        uint16_t set_type = cache_organiser.get_set_type(set);

        if (set_type == COLD || set_type == VERY_COLD) // for cold and very cold sets
        {
            if (block[set][way].foreign) // if block is foreign
            {
                // update lru replacement state of foreign blocks in cold set
                for (uint32_t i = 0; i < NUM_WAY; i++)
                {
                    if (block[set][i].foreign && block[set][i].lru < block[set][way].lru)
                    {
                        block[set][i].lru++;
                    }
                }

                int32_t parent = cache_organiser.get_parent_set(set); // get parent of cold set

                if (parent != -1)
                {
                    // update lru replacement state of blocks in parent set
                    for (uint32_t i = 0; i < NUM_WAY; i++)
                    {
                        if (block[parent][i].foreign == false && block[parent][i].lru < block[set][way].lru)
                        {
                            block[parent][i].lru++;
                        }
                    }
                }
                else
                {
                    // Error handling
                    cerr << "Error at llc_update_replacement_state in getting parent set" << endl;
                    assert(0);
                }
            }
            else // if block is not foreign
            {
                // update lru replacement state
                for (uint32_t i = 0; i < NUM_WAY; i++)
                {
                    if (block[set][i].foreign == false && block[set][i].lru < block[set][way].lru)
                    {
                        block[set][i].lru++;
                    }
                }
            }

            block[set][way].lru = 0; // promote to the MRU position
        }
        else // for hot and very hot sets
        {
            // update lru replacement state of hot set
            for (uint32_t i = 0; i < NUM_WAY; i++)
            {
                if (block[set][i].foreign == false && block[set][i].lru < block[set][way].lru)
                {
                    block[set][i].lru++;
                }
            }

            int32_t helper = cache_organiser.get_helper_set(set); // get helper set

            if (helper != -1)
            {
                // update lru replacement state of helper set
                for (uint32_t i = 0; i < NUM_WAY; i++)
                {
                    if (block[set][i].foreign && block[set][i].lru < block[set][way].lru)
                    {
                        block[set][i].lru++;
                    }
                }
            }
            else
            {
                // Error handling
                cerr << "Error at llc_update_replacement_state in getting helper set" << endl;
                assert(0);
            }

            block[set][way].lru = 0; // promote to the MRU position
        }
    }
    else // before warmup
    {
        return lru_update(set, way);
    }
}
//Change
void CACHE::llc_replacement_final_stats()
{
}
