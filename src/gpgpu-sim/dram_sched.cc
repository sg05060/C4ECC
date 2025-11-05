// Copyright (c) 2009-2011, Tor M. Aamodt, Ali Bakhoda, George L. Yuan,
// The University of British Columbia
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution. Neither the name of
// The University of British Columbia nor the names of its contributors may be
// used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "dram_sched.h"
#include "../abstract_hardware_model.h"
#include "gpu-misc.h"
#include "gpu-sim.h"
#include "mem_latency_stat.h"

frfcfs_scheduler::frfcfs_scheduler(const memory_config *config, dram_t *dm,
                                   memory_stats_t *stats) {
  m_config = config;
  m_stats = stats;
  m_num_pending = 0;
  m_num_write_pending = 0;
  m_dram = dm;
  m_queue = new std::list<dram_req_t *>[m_config->nbk];
  m_bins = new std::map<
      unsigned, std::list<std::list<dram_req_t *>::iterator> >[m_config->nbk];
  m_last_row =
      new std::list<std::list<dram_req_t *>::iterator> *[m_config->nbk];
  curr_row_service_time = new unsigned[m_config->nbk];
  row_service_timestamp = new unsigned[m_config->nbk];
  for (unsigned i = 0; i < m_config->nbk; i++) {
    m_queue[i].clear();
    m_bins[i].clear();
    m_last_row[i] = NULL;
    curr_row_service_time[i] = 0;
    row_service_timestamp[i] = 0;
  }
  if (m_config->seperate_write_queue_enabled) {
    m_write_queue = new std::list<dram_req_t *>[m_config->nbk];
    m_write_bins = new std::map<
        unsigned, std::list<std::list<dram_req_t *>::iterator> >[m_config->nbk];
    m_last_write_row =
        new std::list<std::list<dram_req_t *>::iterator> *[m_config->nbk];

    for (unsigned i = 0; i < m_config->nbk; i++) {
      m_write_queue[i].clear();
      m_write_bins[i].clear();
      m_last_write_row[i] = NULL;
    }
  }
  m_mode = READ_MODE;
}

void frfcfs_scheduler::add_req(dram_req_t *req) {
  if (m_config->seperate_write_queue_enabled && req->data->is_write()) {
    assert(m_num_write_pending < m_config->gpgpu_frfcfs_dram_write_queue_size);
    m_num_write_pending++;
    m_write_queue[req->bk].push_front(req);
    std::list<dram_req_t *>::iterator ptr = m_write_queue[req->bk].begin();
    m_write_bins[req->bk][req->row].push_front(ptr);  // newest reqs to the
                                                      // front
  } else {
    assert(m_num_pending < m_config->gpgpu_frfcfs_dram_sched_queue_size);
    m_num_pending++;
    m_queue[req->bk].push_front(req);
    std::list<dram_req_t *>::iterator ptr = m_queue[req->bk].begin();
    m_bins[req->bk][req->row].push_front(ptr);  // newest reqs to the front
    //sg05060
    //if(req) {
    //  if(req->data->get_request_uid() == 3828593) {
    //    printf("[PSH_DEBUG] sched->add_req in scheduler. uid : %d, is_redun : %d, bk : %d, row : %d, is_write : %d\n",req->data->get_request_uid(), 
    //    req->data->get_is_redundancy(), req->bk, req->row, req->data->is_write());
    //  }
    //}
  }
}

void frfcfs_scheduler::data_collection(unsigned int bank) {
  if (m_dram->m_gpu->gpu_sim_cycle > row_service_timestamp[bank]) {
    curr_row_service_time[bank] =
        m_dram->m_gpu->gpu_sim_cycle - row_service_timestamp[bank];
    if (curr_row_service_time[bank] >
        m_stats->max_servicetime2samerow[m_dram->id][bank])
      m_stats->max_servicetime2samerow[m_dram->id][bank] =
          curr_row_service_time[bank];
  }
  curr_row_service_time[bank] = 0;
  row_service_timestamp[bank] = m_dram->m_gpu->gpu_sim_cycle;
  if (m_stats->concurrent_row_access[m_dram->id][bank] >
      m_stats->max_conc_access2samerow[m_dram->id][bank]) {
    m_stats->max_conc_access2samerow[m_dram->id][bank] =
        m_stats->concurrent_row_access[m_dram->id][bank];
  }
  m_stats->concurrent_row_access[m_dram->id][bank] = 0;
  m_stats->num_activates[m_dram->id][bank]++;
}

dram_req_t *frfcfs_scheduler::schedule(unsigned bank, unsigned curr_row) {
  // row
  bool rowhit = true;
  std::list<dram_req_t *> *m_current_queue = m_queue;
  std::map<unsigned, std::list<std::list<dram_req_t *>::iterator> >
      *m_current_bins = m_bins;
  std::list<std::list<dram_req_t *>::iterator> **m_current_last_row =
      m_last_row;

  if (m_config->seperate_write_queue_enabled) {
    if (m_mode == READ_MODE &&
        ((m_num_write_pending >= m_config->write_high_watermark)
         // || (m_queue[bank].empty() && !m_write_queue[bank].empty())
         )) {
      m_mode = WRITE_MODE;
    } else if (m_mode == WRITE_MODE &&
               ((m_num_write_pending < m_config->write_low_watermark)
                //  || (!m_queue[bank].empty() && m_write_queue[bank].empty())
                )) {
      m_mode = READ_MODE;
    }
  }

  if (m_mode == WRITE_MODE) {
    m_current_queue = m_write_queue;
    m_current_bins = m_write_bins;
    m_current_last_row = m_last_write_row;
  }
  
  /* sg05060: Debug */
  const unsigned TARGET_UID = 3828593u;
  const unsigned TARGET_ROW = 4378u; // 또는 4378u
  auto bin_it = m_bins[bank].find(TARGET_ROW);
  if (bin_it != m_bins[bank].end()) {
    // bin에는 m_queue[bank]를 가리키는 iterator들이 들어있음
    bool found = false;
    for (auto it2 = bin_it->second.begin(); it2 != bin_it->second.end(); ++it2) {
      auto qit = *it2;                            // iterator into m_queue[bank]
      if (qit == m_queue[bank].end()) continue;   // 방어
      dram_req_t* r = *qit;
      if (!r || !r->data) continue;
      if (r->data->get_request_uid() == TARGET_UID) { found = true; break; }
    }

    if (found) {
      printf("\n[PSH_DEBUG][READ_BIN] dram=%u bank=%u row=%u entries=%zu\n",
              m_dram->id, bank, TARGET_ROW, bin_it->second.size());
      printf("  ord |    uid | rdd | wr  |  col | bk\n");
      printf("  ----+--------+-----+-----+------+---\n");
      unsigned ord = 0;
      for (auto it2 = bin_it->second.begin(); it2 != bin_it->second.end(); ++it2, ++ord) {
        auto qit = *it2;                          // iterator into m_queue[bank]
        if (qit == m_queue[bank].end()) continue; // 방어
        dram_req_t* r = *qit;
        if (!r || !r->data) continue;

        unsigned uid = r->data->get_request_uid();
        int rdd      = (int)r->data->get_is_redundancy();
        int wr       = (int)r->data->is_write();
        printf("  %3u | %6u |  %d  |  %d  | %4u | %2u%s\n",
                ord, uid, rdd, wr, r->col, r->bk,
                (uid==TARGET_UID ? "  <- TARGET" : ""));
      }
    }
  }


  if (m_current_last_row[bank] == NULL) {
    if (m_current_queue[bank].empty()) return NULL;

    std::map<unsigned, std::list<std::list<dram_req_t *>::iterator> >::iterator
        bin_ptr = m_current_bins[bank].find(curr_row);
    if (bin_ptr == m_current_bins[bank].end()) {
      dram_req_t *req = m_current_queue[bank].back();
      bin_ptr = m_current_bins[bank].find(req->row);
      assert(bin_ptr !=
             m_current_bins[bank].end());  // where did the request go???
      m_current_last_row[bank] = &(bin_ptr->second);
      data_collection(bank);
      rowhit = false;
    } else {
      m_current_last_row[bank] = &(bin_ptr->second);
      rowhit = true;
    }
  }
  std::list<dram_req_t *>::iterator next = m_current_last_row[bank]->back();
  dram_req_t *req = (*next);

  //sg05060:251103_RMW
  //if(req && req->data) {
  //  mem_fetch* pending_mf = req->data;
  //  if(pending_mf->is_write() && pending_mf->get_is_need_rdd()) {
  //    rmw_state_t st = pending_mf->get_rmw_state();
  //      if (st == RMW_WAIT_RD) {
  //        return NULL;
  //      } 
  //      else if(st == RMW_WAIT_WR_RDD) {
  //        pending_mf->set_rmw_state(RMW_READY_ORIG);
  //        auto *shadow_wr_req = m_dram->spawn_shadow_req(req, pending_mf->get_rmw_id(), 1);
  //        //printf("[RMW][spawn] parent=%p shadow=%p rmw_id=%u\n",
  //        //      req->data, shadow_wr_req->data, pending_mf->get_rmw_id());
  //        //printf("    flags: internal=%d srd=%d parent_set=%d\n",
  //        //      (int)shadow_wr_req->data->is_rmw_internal_req(),
  //        //      (int)shadow_wr_req->data->is_rmw_shadow_read(),
  //        //      (shadow_wr_req->data->get_rmw_parent()!=nullptr));
  //        assert(shadow_wr_req->data->get_rmw_parent());
  //        return shadow_wr_req;
  //      } 
  //      else if(st == RMW_NONE) {
  //        static unsigned rmw_seq_num = 1u;
  //        unsigned rmw_id = rmw_seq_num++;
  //        pending_mf->set_rmw_state(RMW_WAIT_RD);
  //        pending_mf->set_rmw_id(rmw_id);
  //        auto *shadow_rd_req = m_dram->spawn_shadow_req(req, rmw_id, 0);
  //        //printf("[RMW][spawn] parent=%p shadow=%p rmw_id=%u\n",
  //        //      req->data, shadow_rd_req->data, rmw_id);
  //        //printf("    flags: internal=%d srd=%d parent_set=%d\n",
  //        //      (int)shadow_rd_req->data->is_rmw_internal_req(),
  //        //      (int)shadow_rd_req->data->is_rmw_shadow_read(),
  //        //      (shadow_rd_req->data->get_rmw_parent()!=nullptr));
  //        assert(shadow_rd_req->data->get_rmw_parent());
  //        return shadow_rd_req;
  //      } 
  //      else if(st == RMW_READY_ORIG) {
  //        //continue;
  //      }
  //  }
  //}

  // rowblp stats
  m_dram->access_num++;
  bool is_write = req->data->is_write();
  if (is_write)
    m_dram->write_num++;
  else
    m_dram->read_num++;

  if (rowhit) {
    m_dram->hits_num++;
    if (is_write)
      m_dram->hits_write_num++;
    else
      m_dram->hits_read_num++;
  }

  m_stats->concurrent_row_access[m_dram->id][bank]++;
  m_stats->row_access[m_dram->id][bank]++;
  m_current_last_row[bank]->pop_back();

  m_current_queue[bank].erase(next);
  if (m_current_last_row[bank]->empty()) {
    m_current_bins[bank].erase(req->row);
    m_current_last_row[bank] = NULL;
  }
#ifdef DEBUG_FAST_IDEAL_SCHED
  if (req)
    printf("%08u : DRAM(%u) scheduling memory request to bank=%u, row=%u\n",
           (unsigned)gpu_sim_cycle, m_dram->id, req->bk, req->row);
#endif

  if (m_config->seperate_write_queue_enabled && req->data->is_write()) {
    assert(req != NULL && m_num_write_pending != 0);
    m_num_write_pending--;
  } else {
    assert(req != NULL && m_num_pending != 0);
    m_num_pending--;
  }

  return req;
}

void frfcfs_scheduler::print(FILE *fp) {
  for (unsigned b = 0; b < m_config->nbk; b++) {
    printf(" %u: queue length = %u\n", b, (unsigned)m_queue[b].size());
  }
}

void dram_t::scheduler_frfcfs() {
  unsigned mrq_latency;
  frfcfs_scheduler *sched = m_frfcfs_scheduler;
  while (!mrqq->empty()) {
    dram_req_t *req = mrqq->pop();
    
    //sg05060
    //if(req->data->get_request_uid() == 3828593) {
    //  printf("[PSH_DEBUG] mrqq->pop in scheduler. uid : %d, is_redun : %d\n",req->data->get_request_uid(), 
    //  req->data->get_is_redundancy());
    //}

    // Power stats
    // if(req->data->get_type() != READ_REPLY && req->data->get_type() !=
    // WRITE_ACK)
    m_stats->total_n_access++;

    if (req->data->get_type() == WRITE_REQUEST) {
      m_stats->total_n_writes++;
    } else if (req->data->get_type() == READ_REQUEST) {
      m_stats->total_n_reads++;
    }

    req->data->set_status(IN_PARTITION_MC_INPUT_QUEUE,
                          m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
    sched->add_req(req);
  }

  dram_req_t *req;
  unsigned i;
  for (i = 0; i < m_config->nbk; i++) {
    unsigned b = (i + prio) % m_config->nbk;
    if (!bk[b]->mrq) {
      req = sched->schedule(b, bk[b]->curr_row);
      //sg05060
      //if(req != nullptr) {
      //  if(req->data->get_request_uid() == 3828593) {
      //  printf("[PSH_DEBUG] sched->pop in scheduler. uid : %d, is_redun : %d\n",req->data->get_request_uid(), 
      //  req->data->get_is_redundancy());
      //  }
      //}
      //if((req != nullptr) && (this->id == 0) && (b == 4)) {
      //  printf("[PSH_DEBUG] Bank[4] Deteced\n");
      //}

      if (req) {
        req->data->set_status(IN_PARTITION_MC_BANK_ARB_QUEUE,
                              m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle);
        prio = (prio + 1) % m_config->nbk;
        bk[b]->mrq = req;
        if (m_config->gpgpu_memlatency_stat) {
          mrq_latency = m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle -
                        bk[b]->mrq->timestamp;
          m_stats->tot_mrq_latency += mrq_latency;
          m_stats->tot_mrq_num++;
          bk[b]->mrq->timestamp =
              m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle;
          m_stats->mrq_lat_table[LOGB2(mrq_latency)]++;
          if (mrq_latency > m_stats->max_mrq_latency) {
            m_stats->max_mrq_latency = mrq_latency;
          }
        }

        break;
      }
    }
  }
}
