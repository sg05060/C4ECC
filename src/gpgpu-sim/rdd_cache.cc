#include "mem_fetch.h"
#include "gpu-sim.h"
#include "rdd_cache.h"


RDD_CACHE_STATE redundancy_cache::access(mem_fetch* mf)
{
    //unsigned index = mf->get_rdd_tag() & m_index_mask;
    //unsigned tag = mf->get_rdd_tag() >> m_index_bits;
    const unsigned tag = mf->get_rdd_tag();
    RDD_CACHE_STATE state = RDD_MISS;
   
    // Redundancy Hit
    
    //if (mf->get_rdd_tag() == rcache.tag){
    //    ++hit_count;
    //    return RDD_HIT;
    //}

    //if(m_lines[index].valid && (m_lines[index].tag == tag)) {
    //    ++hit_count;
    //    return RDD_HIT;
    //}
    for (const auto &line : m_lines) {
        if (line.valid && (line.tag == tag)) {
            ++hit_count;
            return RDD_HIT;
        }
    }

    for (auto it = mshr.begin(); it != mshr.end(); ++it)
    {
        rdd_mshr_entry& mshr_entry = *it;
        if (mshr_entry.valid == false) continue;
        
        // Redundancy Pending-Hit
        if (tag == mshr_entry.tag){
            if(!mshr_entry.merge_finished && (mshr_entry.pending_num < max_merge)){
                state = RDD_PENDING_HIT;
                mshr_entry.data_requests.push_back(mf);
                if(++mshr_entry.pending_num == max_merge) mshr_entry.merge_finished = true;
                //printf("Pending HIT: %u,  %d\n", mshr_entry.pending_request,  mshr_entry.pending_num);
                mf->set_redundancy_pair(mshr_entry.pending_request);
                ++pending_hit_count;
                return state;
            } else {
                break;
            }
        }
    }

   if (state == RDD_MISS){
       ++miss_count;
       return state;
   }
   else{
       assert(0);
   }
}

void redundancy_cache::update_cache(mem_fetch* mf){
    const unsigned tag = mf->get_rdd_tag();
    //unsigned index = mf->get_rdd_tag() & m_index_mask;
    //unsigned tag = mf->get_rdd_tag() >> m_index_bits;
    //m_lines[index].valid = true;
    //m_lines[index].tag   = tag;
    for (auto &line : m_lines) {
        if (!line.valid) {
            line.valid = true;
            line.tag   = tag;
            return;
        }
    }

    unsigned vic = pick_victim();
    m_lines[vic].valid = true;
    m_lines[vic].tag   = tag;
}

void redundancy_cache::push_mshr(mem_fetch* mf){ // hj argument modified
    if (mshr.size() < mshr_size){
        const unsigned tag = mf->get_rdd_tag();
        //printf("i wanna go home {%d}\n",mshr.size());
        //unsigned index = mf->get_rdd_tag() & m_index_mask;
        //unsigned tag = mf->get_rdd_tag() >> m_index_bits;

        rdd_mshr_entry rdd_mshr_info;
        rdd_mshr_info.valid = true;
        rdd_mshr_info.tag = tag;
        rdd_mshr_info.pending_request = mf;
        rdd_mshr_info.pending_num = 1;
        rdd_mshr_info.merge_finished = false; // jyk
        rdd_mshr_info.data_requests.push_back(mf); // hojung add
        mshr.push_back(rdd_mshr_info);
    }
}

void redundancy_cache::cleanup_invalid_entries(){
    //int x = 0;
    if (mshr.size() >= mshr_size){
        //printf("mshr full! attempt replace\n");
        for (auto it = mshr.begin(); it != mshr.end();){
            if(!(it->valid)){
                //printf("pending request deletion success\n");
                it = mshr.erase(it);
                //printf("mshr entry deletion success\n");
            } else {
                ++it;

                // debug
                //printf("iterate.. %d\n", x);
                //x++;
            }
        }
    }
}

void redundancy_cache::print_rdd_statistics() const{
    std::cout << "Redundancy Cache Hit Count: " << get_hit_count() << std::endl;
    std::cout << "Redundancy Cache Pending Hit Count: " << get_pending_hit_count() << std::endl;
    std::cout << "Redundancy Cache Miss Count: " << get_miss_count() << std::endl;
}

// jyk
void redundancy_cache::processing_mshr(mem_fetch* mf) {
    for(auto& entry : mshr) {
        if((entry.pending_request == mf) && entry.valid) {
            // Find it!
            for(auto it = entry.data_requests.begin(); it != entry.data_requests.end(); it++) {
                (*it)->set_redundancy_pair(nullptr);
            }
            entry.valid = false;
            return;
        }
    }
} 

RDD_MSHR_STATUS redundancy_cache::update_mshr_status(mem_fetch* mf){
    RDD_MSHR_STATUS status = NO_MATCH;
    for (auto& entry : mshr){
        if ((entry.pending_request == mf) && entry.valid){
            status = MATCH_PENDING;
            //printf("Enter Pending HIT...: %u, %d\n", entry.pending_request, entry.pending_num);
            if (--entry.pending_num == 0){
                status = MATCH_FINISHED;
                //printf("Exit Pending HIT...: %u, %d\n", entry.pending_request, entry.pending_num);
                entry.valid = false;
            }
            break;
        }
    }
    return status;
}
