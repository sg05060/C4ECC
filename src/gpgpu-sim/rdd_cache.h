#ifndef RDD_CACHE
#define RDD_CACHE

    #include <vector>
    #include "mem_fetch.h"

    enum RDD_CACHE_STATE
    {
        RDD_HIT,
        RDD_PENDING_HIT,
        RDD_MISS,
    };

    enum RDD_MSHR_STATUS
    {
        NO_MATCH,
        MATCH_PENDING,
        MATCH_FINISHED,
    };

    typedef struct RDD_CACHE_ENTRY{
        bool valid;
        unsigned tag;
        //mem_fetch* data;
    }rdd_cache_entry;

    typedef struct RDD_MSHR_ENTRY{
        bool valid;
        unsigned tag;
        mem_fetch* pending_request;
        unsigned pending_num;

        // jyk
        std::vector<mem_fetch*> data_requests;
        bool merge_finished;
    }rdd_mshr_entry;

    class redundancy_cache {
        private:
            //rdd_cache_entry rcache;
            std::vector <rdd_cache_entry> m_lines;
            unsigned m_num_lines;
            unsigned m_index_bits;
            unsigned m_index_mask;

            unsigned mshr_size;
            unsigned max_merge; // jyk
            std::vector <rdd_mshr_entry> mshr;

            unsigned hit_count;
            unsigned pending_hit_count;
            unsigned miss_count;
            
            std::mt19937 m_rng; // for Random Replacement
            
            inline unsigned pick_victim() {
                if (m_lines.empty()) return 0;
                std::uniform_int_distribution<unsigned> dist(0, (unsigned)m_lines.size()-1);
                return dist(m_rng);
            }

        public:
            redundancy_cache(unsigned num_lines = 1,
                             unsigned num_mshr_sz = 2)
            : m_num_lines(num_lines),
              mshr_size(num_mshr_sz),
              m_rng(0xC0FFEEu) 
            {
                //rcache.tag = -1;
                //rcache.data = nullptr;
                m_index_bits = (unsigned)std::log2((double)m_num_lines);
                m_index_mask = m_num_lines - 1;

                m_lines.assign(m_num_lines, rdd_cache_entry{false,0});
                mshr.clear();
                //mshr_size = 2;
                hit_count = 0;
                pending_hit_count = 0;
                miss_count = 0;
                max_merge = 4; // jyk
            };

            ~redundancy_cache();

            RDD_CACHE_STATE access(mem_fetch* mf);
            
            void update_cache(mem_fetch* mf);

            void push_mshr(mem_fetch* mf);

            unsigned get_hit_count() const { return hit_count; }
            unsigned get_pending_hit_count() const { return pending_hit_count; }
            unsigned get_miss_count() const { return miss_count; }
            void  set_valid_bit(unsigned index, bool valid){
                if (index < mshr.size()){
                    mshr[index].valid = valid;
                }
            }

            void cleanup_invalid_entries();
            RDD_MSHR_STATUS update_mshr_status(mem_fetch* mf);
            void processing_mshr(mem_fetch* mf); // jyk
            void print_rdd_statistics() const;

    };

#endif
