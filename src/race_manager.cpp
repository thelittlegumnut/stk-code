//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include <iostream>

#include "track_manager.hpp"
#include "game_manager.hpp"
#include "kart_properties_manager.hpp"
#include "race_manager.hpp"
#include "unlock_manager.hpp"
#include "gui/menu_manager.hpp"
#include "world.hpp"
#include "scene.hpp"
#include "user_config.hpp"
#include "stk_config.hpp"


RaceManager* race_manager= NULL;

RaceManager::RaceManager()
{
    m_num_karts          = user_config->m_karts;
    m_difficulty         = RD_HARD;
    m_major_mode         = RM_SINGLE;
    m_minor_mode         = RM_QUICK_RACE;
    m_track_number       = 0;
    m_active_race        = false;
    m_score_for_position = stk_config->m_scores;
    m_coin_target        = 0;
    setTrack("race");
    setLocalPlayerKart(0, "tuxkart");
}   // RaceManager

//-----------------------------------------------------------------------------
RaceManager::~RaceManager()
{
}   // ~RaceManager

//-----------------------------------------------------------------------------
void RaceManager::reset()
{
    m_num_finished_karts   = 0;
    m_num_finished_players = 0;
}  // reset

//-----------------------------------------------------------------------------
void RaceManager::setLocalPlayerKart(unsigned int player, const std::string& kart)
{

    if (player >= 0 && player < 4)
    {
        if (player >= getNumLocalPlayers())
            setNumLocalPlayers(player+1);
        m_local_player_karts[player] = kart;

    }
    else
    {
        fprintf(stderr, "Warning: player '%d' does not exists.\n", player);
    }
}   // setLocalPlayerKart
//-----------------------------------------------------------------------------
void RaceManager::setPlayerKart(unsigned int player, const std::string& kart,
                                const std::string& player_name, int host_id)
{
    if (player >= getNumPlayers())
        setNumPlayers(player+1);
    m_player_karts[player]  = kart;
    m_player_names[player]  = player_name;
}   // setPlayerKart

//-----------------------------------------------------------------------------
void RaceManager::setNumPlayers(int num)
{
    m_player_karts.resize(num);
    m_player_names.resize(num);
    for(PlayerKarts::iterator i = m_player_karts.begin(); i != m_player_karts.end(); ++i)
        if (i->empty()) *i = "tuxkart";
    for(PlayerKarts::iterator i = m_player_names.begin(); i != m_player_names.end(); ++i)
        if (i->empty()) *i = "guest";

}   // setNumPlayers

//-----------------------------------------------------------------------------
void RaceManager::setDifficulty(Difficulty diff)
{
    if(diff==RD_SKIDDING)
    {
        m_difficulty = RD_HARD;
        user_config->m_skidding = true;
    }
    else
    {
        m_difficulty = diff;
        user_config->m_skidding = false;
    }
}   // setDifficulty

//-----------------------------------------------------------------------------
void RaceManager::setTrack(const std::string& track)
{
    m_tracks.clear();
    m_tracks.push_back(track);
}   // setTrack

//-----------------------------------------------------------------------------
void RaceManager::startNew()
{
    if(m_major_mode==RM_GRAND_PRIX)   // GP: get tracks and laps from grand prix
    {
        m_tracks   = m_grand_prix.getTracks();
        m_num_laps = m_grand_prix.getLaps();
    }
    assert(m_player_karts.size() > 0);

    // command line parameters: negative numbers=all karts
    if(m_num_karts < 0 ) m_num_karts = stk_config->m_max_karts;
    if((size_t)m_num_karts < m_player_karts.size()) 
        m_num_karts = (int)m_player_karts.size();

    // Create the list of all kart names to use
    // ========================================
    std::vector<std::string> kart_names;
    kart_names.resize(m_num_karts);
    for(unsigned int i = 0; i < m_player_karts.size(); i++)
    {
        /*Players position is behind the AI in the first race*/
        kart_names[m_num_karts-1 - i] = m_player_karts[m_player_karts.size() - 1 - i];
    }
    kart_properties_manager->fillWithRandomKarts(kart_names);

    // Create the kart status data structure to keep track of scores, times, ...
    // ==========================================================================
    const int num_ai_karts = m_num_karts - (int)m_player_karts.size();
    m_kart_status.clear();
    for(int i=0; i<m_num_karts; i++)
    {
        // AI karts have -1 as player 
        bool is_player = i>=num_ai_karts;   // players start at the back
        m_kart_status.push_back(KartStatus(kart_names[i], i,
                                           is_player ? i-num_ai_karts : -1,
                                           is_player ? KT_PLAYER: KT_AI
                                           ) );
    }   // for i<m_num_karts

    // Then start the race with the first track
    // ========================================
    m_track_number = 0;
    startNextRace();
}   // startNew

//-----------------------------------------------------------------------------
void RaceManager::startNextRace()
{

    m_num_finished_karts   = 0;
    m_num_finished_players = 0;

    // if subsequent race, sort kart status structure
    // ==============================================
    if (m_track_number > 0)
    {  
        // In follow the leader mode do not change the first kart, 
        // since it's always the leader.
        int offset = (m_minor_mode==RM_FOLLOW_LEADER) ? 1 : 0;

        std::sort(m_kart_status.begin()+offset, m_kart_status.end());
        //reverse kart order if flagged in stk_config
        if (stk_config->m_grid_order)
        {
            std::reverse(m_kart_status.begin()+offset, m_kart_status.end());
        } 
    }   // not first race

    // the constructor assigns this object to the global
    // variable world. Admittedly a bit ugly, but simplifies
    // handling of objects which get created in the constructor
    // and need world to be defined.
    new World();

    m_active_race = true;
}   // startNextRace

//-----------------------------------------------------------------------------
void RaceManager::next()
{
    m_num_finished_karts   = 0;
    m_num_finished_players = 0;
    m_track_number++;
    if(m_track_number<(int)m_tracks.size())
    {
        scene->clear();
        startNextRace();
    }
    else
    {
        exit_race();
    }
}   // next

//-----------------------------------------------------------------------------
void RaceManager::exit_race()
{
    // Only display the grand prix result screen if all tracks 
    // were finished, and not when a race is aborted.
    if(m_major_mode==RM_GRAND_PRIX && m_track_number==(int)m_tracks.size()) 
    {
        unlock_manager->grandPrixFinished();
        menu_manager->switchToGrandPrixEnding();
    }
    else
    {
        menu_manager->switchToMainMenu();
    }
    scene->clear();
    delete world;
    world          = 0;
    m_track_number = 0;
    m_active_race  = false;    
}   // exit_Race

//-----------------------------------------------------------------------------
// Kart kart has finished the race at the specified time (which can be different
// from world->getClock() in case of setting extrapolated arrival times).
void RaceManager::RaceFinished(const Kart *kart, float time)
{
    unsigned i;
    for(i=0; i<m_kart_status.size(); i++)
    {
        if(kart->getIdent()==m_kart_status[i].m_ident) break;
    }   // for i
    if(i>=m_kart_status.size())
    {
        fprintf(stderr, "Kart '%s' not found. Ignored.\n",kart->getName().c_str());
        return;
    }

    // In follow the leader mode, kart 0 does not get any points,
    // so the position of each kart is actually one better --> decrease pos
    int pos = kart->getPosition();
    if(m_minor_mode==RM_FOLLOW_LEADER) 
    {
        pos--;
        // If the position is negative (i.e. follow leader and kart on 
        // position 0) set the score of this kart to the lowest possible
        // score, since the kart is ahead of the leader
        if(pos<=0) pos=stk_config->m_max_karts;
    }

    m_kart_status[i].m_score        += m_score_for_position[pos-1];
    m_kart_status[i].m_last_score    = m_score_for_position[pos-1];
    m_kart_status[i].m_overall_time += time;
    m_kart_status[i].m_last_time     = time;
    m_num_finished_karts ++;
    if(kart->isPlayerKart()) m_num_finished_players++;
}   // raceFinished

//-----------------------------------------------------------------------------
void RaceManager::rerunRace()
{
    // Subtract last score from all karts:
    for(int i=0; i<m_num_karts; i++)
    {
        m_kart_status[i].m_score        -= m_kart_status[i].m_last_score;
        m_kart_status[i].m_overall_time -= m_kart_status[i].m_last_time;
    }
    world->restartRace();
}   // rerunRace

/* EOF */
