
#include <iostream>
#include <fstream>

#include <thread>
#include <atomic>
#include <mutex>

#include <ncurses.h>
#include "opz_rtmidi.h"

std::string version = "0.1";
std::string name = "opz_companion";
std::string header = name + " " + version + " by Patricio Gonzalez Vivo ( patriciogonzalezvivo.com )"; 

std::atomic<bool> keepRunnig(true);

std::string hBar(size_t _width, size_t _value) {
    std::string rta = "";
    size_t l = (_value/254.0) * _width;
    for (size_t i = 0; i < _width; i++)
        rta += (i < l ) ? "#" : ".";
    return rta;
}

int main(int argc, char** argv) {

    T3::opz_rtmidi opz;
    opz.connect();
    
    initscr();
    start_color();

    init_color(COLOR_BLACK, 0, 0, 0);
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);

    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    bool change = true;
    bool change_data = true;
    bool pressing_track = false;
    bool pressing_project = false;
    bool pressing_mixer = false;
    bool pressing_tempo = false;
    bool mic_on = false;

    // Listen to key events (no cc, neighter notes)
    opz.setEventCallback( [&](T3::opz_event_id _id, int _value) {
        change = true;

        if (_id == T3::KEY_TRACK)           pressing_track = _value;
        else if (_id == T3::KEY_PROJECT)    pressing_project = _value;
        else if (_id == T3::KEY_MIXER)      pressing_mixer = _value;
        else if (_id == T3::KEY_TEMPO)      pressing_tempo = _value;
        else if (_id == T3::MICROPHONE_MODE_CHANGE) mic_on = _value != 0;
        else if (_id == T3::PATTERN_DOWNLOADED || _id == T3::PATTERN_CHANGE || _id == T3::TRACK_CHANGE || _id == T3::SEQUENCE_CHANGE || _id == T3::PAGE_CHANGE || _id == T3::PARAMETER_CHANGE ) change_data = true;
    } );

    std::thread waitForKeys([&](){
        char ch;
        while ( true ) {
            ch = getch();
            if (ch == 'x') {
                keepRunnig = false;
                keepRunnig.store(false);
                break;
            }
        }
    });

    int y_beg, x_beg, y_max, x_max;

    std::vector<WINDOW*> windows = {
        newwin(5, 41, 1, 0),    //  PAGE ONE
        newwin(4, 41, 6, 0),    //  PAGE TWO
        newwin(5, 41, 10, 0),   //  PAGE THREE
        newwin(14, 25, 1, 41),  //  PAGE FOUR
        newwin(14, 15, 1, 65),  //  STEP / NOTE
        newwin(18, 80, 1, 0),   //  PROJECT
    };
    refresh();

    while (keepRunnig.load()) {
        getbegyx(stdscr, y_beg, x_beg);
        getmaxyx(stdscr, y_max, x_max);

        opz.keepawake();

        if (!change)
            continue;

        size_t project_id = opz.getActiveProjectId();
        uint8_t pattern_id = opz.getActivePatternId();
        T3::opz_track_id track_id = opz.getActiveTrackId();
        T3::opz_project_data project = opz.getProjectData();
        T3::opz_pattern pattern = opz.getActivePattern();
        int x_width = 4;

        std::string title_name =  T3::toString(track_id);

        if (mic_on) title_name = "MICROPHONE";
        else if (pressing_project) title_name = "PROJECTS";
        else if (pressing_mixer)   title_name = "MIXER";
        else if (pressing_tempo)   title_name = "TEMPO";

        clear();
        mvprintw(0, (x_max-x_beg)/2 - title_name.size()/2, "%s", title_name.c_str() );

        for (size_t i = 0; i < 16; i++) {
            size_t x = 3 + i * x_width + ( (i/4) * x_width);
            mvprintw(y_max-5, x, "%02i", i + 1 );
            size_t note = opz.getNoteIdOffset(track_id, i);

            if ( pattern.note[ note ].note == 0xFF)
                mvprintw(y_max-4, x, "-");
            else {
                attron(COLOR_PAIR(2));
                mvprintw(y_max-4, x, "o");
                attroff(COLOR_PAIR(2));
            }
        }
        
        mvprintw(y_max-2, 0, "STEP COUNT %2i      STEP LENGHT %2i                                        SUM %2i", 
                                opz.getActiveTrackParameters().step_count, 
                                opz.getActiveTrackParameters().step_length,
                                opz.getActiveTrackParameters().step_count * opz.getActiveTrackParameters().step_length);
        mvprintw(y_max-1, (x_max-x_beg)/2 - 3, "%s %02i", ((opz.isPlaying())? "|> " : "[ ]"), opz.getActiveStepId() + 1 );
        refresh();

        if (mic_on) {
            wclear(windows[5]);
            box(windows[5], 0, 0);
            mvwprintw(windows[5], 1, 2, "MIC LEVEL                  MIC FX ");
            mvwprintw(windows[5], 2, 2, "%s                  %s", 
                                                                    hBar(9, opz.getMicLevel() ).c_str(), 
                                                                    T3::toString(opz.getMicFx()).c_str());
            wrefresh(windows[5]);
        }
        else if (pressing_project) {
            wclear(windows[5]);
            box(windows[5], 0, 0);

            mvwprintw(windows[5], 0, 2, " PROJECT  %02i ", project_id);
            mvwprintw(windows[5], 0, 20, " PATTERN  %02i ", pattern_id );
            
            for (size_t y = 0; y < 16; y++) {
                // mvwprintw(windows[5], 1, 12 + y * x_width, "%02i ", y+1);

                if (y == track_id)
                    wattron(windows[5], COLOR_PAIR(2));

                mvwprintw(windows[5], y+1, 2, "%7s", T3::toString( T3::opz_track_id(y) ).c_str() );

                // mvwprintw(windows[5], x+2 , 10 + 16 * x_width, "%i", pattern.track_param[x].step_count );
                // mvwprintw(windows[5], x+2 , 10 + 16 * x_width + 5, "%i", pattern.track_param[x].step_length );
                // mvwprintw(windows[5], x+2 , 10 + 16 * x_width + 2, "%i", pattern.track_param[x].note_length );

                for (size_t x = 0; x < 16; x++) {

                    size_t i = opz.getNoteIdOffset(y, x);
                    // mvwprintw(windows[5], y+4, 12 + x * x_width, "%i", i );
                    if ( pattern.note[ i ].note == 0xFF)
                        mvwprintw(windows[5], y+1, 12 + x * x_width, " .");
                    else
                        mvwprintw(windows[5], y+1, 12 + x * x_width, "%02X", pattern.note[ i ].note );
                }
                wattroff(windows[5], COLOR_PAIR(2));
            }
            wrefresh(windows[5]);
        }
        else if (pressing_mixer) {
            wclear(windows[5]);
            box(windows[5], 0, 0);
            mvwprintw(windows[5], 1, 2, "DRUMS               SYNTH                PUNCH                MASTER");
            mvwprintw(windows[5], 2, 2, "%03i                 %03i                  %03i                  %03i", 
                                                                    (int)((int)project.drum_level / 2.55f), 
                                                                    (int)((int)project.synth_level / 2.55f), 
                                                                    (int)((int)project.punch_level / 2.55f), 
                                                                    (int)((int)project.master_level / 2.55f) );
            wrefresh(windows[5]);

        }
        else if (pressing_tempo) {
            wclear(windows[5]);
            box(windows[5], 0, 0);
            mvwprintw(windows[5], 1, 2, "TEMPO               SWING                SOUND                LEVEL");
            mvwprintw(windows[5], 2, 2, "%03i                 %03i                  %03i                  %03i", 
                                                                    project.tempo, 
                                                                    (int)((int)project.swing / 2.55f) - 50, 
                                                                    (int)((int)project.metronome_sound / 2.55f) , 
                                                                    (int)((int)project.metronome_level / 2.55f) );
            wrefresh(windows[5]);
        }

        else {
            wclear(windows[5]);

            if (pressing_track)
                wattron(windows[4], COLOR_PAIR(2));

            size_t page = (size_t)opz.getActivePageId();

            for (size_t i = 0; i < 5; i++) {
                if (!pressing_track && i == page)
                    wattron(windows[i], COLOR_PAIR(2));
                box(windows[i], 0, 0);
                wattroff(windows[i], COLOR_PAIR(2));
            }

            if (page == 0 || change_data) {
                // PAGE 1: SOUND        
                mvwprintw(windows[0], 1, 1, "SOUND  P1      P2      FILTER  RESONA.");
                mvwprintw(windows[0], 2, 1, "       %s %s %s %s",   hBar(7, (size_t)opz.getActivePageParameters().param1).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().param2).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().filter).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().resonance).c_str() );
                mvwprintw(windows[0], 3, 1, "       %7i %7i %7i %7i", 
                                                        (int)((int)opz.getActivePageParameters().param1 / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().param2 / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().filter / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().resonance / 2.55f) );
            }

            if (page == 1 || change_data) {
                // PAGE 2: ENVELOPE
                mvwprintw(windows[1], 1, 1, "ENV.   ATTACK  DECAY   SUSTAIN RELEASE");
                mvwprintw(windows[1], 2, 1, "       %7i %7i %7i %7i",
                                                        (int)((int)opz.getActivePageParameters().attack / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().decay / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().sustain / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().release / 2.55f) );
            }


            if (page == 2 || change_data) {
                // PAGE 3: LFO
                mvwprintw(windows[2],1, 1, "LFO    DEPTH   RATE    DEST    SHAPE");
                mvwprintw(windows[2],2, 1, "       %s %s %s %s",  hBar(7, (size_t)opz.getActivePageParameters().lfo_depth).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().lfo_speed).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().lfo_value).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().lfo_shape).c_str() );
                mvwprintw(windows[2],3, 1, "       %7i %7i %7i %7i", 
                                                        (int)((int)opz.getActivePageParameters().lfo_depth / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().lfo_speed / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().lfo_value / 2.55f), 
                                                        (int)((int)opz.getActivePageParameters().lfo_shape / 2.55f) );
            }

            if (page == 3 || change_data) {
                // PAGE 4: FX / PAN & LEVEL
                mvwprintw(windows[3], 1, 1, " FX  1       2");
                mvwprintw(windows[3], 2, 1, "     %s %s", hBar(7, (size_t)opz.getActivePageParameters().fx1).c_str(),
                                                        hBar(7, (size_t)opz.getActivePageParameters().fx2).c_str());
                mvwprintw(windows[3], 3, 1, "     %7i %7i", 
                                                        (int)((int)opz.getActivePageParameters().fx1 / 2.55f),
                                                        (int)((int)opz.getActivePageParameters().fx2 / 2.55f) );

                mvwprintw(windows[3], 6, 1, " PAN L             R");
                mvwprintw(windows[3], 7, 1, "     ");
                for (size_t i = 0; i < 15; i++) {
                    size_t p = opz.getActivePageParameters().pan;
                    p = (p/254.0)*15;
                    if (i + 2 > p  && i < p ) wprintw(windows[3], "|");
                    else wprintw(windows[3],".");
                }
                
                mvwprintw(windows[3], 10, 1, " LEVEL");
                mvwprintw(windows[3], 11, 1, "     %s", hBar(15, (size_t)opz.getActivePageParameters().level).c_str() );
                mvwprintw(windows[3], 12, 1, "           %03i", (int)( (int)opz.getActivePageParameters().level / 2.55f));
            }

            if (pressing_track || change_data) {
                mvwprintw(windows[4], 1,  2, "NOTE LENGTH");
                mvwprintw(windows[4], 2,  2, "%i", opz.getActiveTrackParameters().note_length);
                mvwprintw(windows[4], 4,  2, "NOTE STYLE");
                mvwprintw(windows[4], 5,  2, "%i", opz.getActivePageParameters().note_style );
                mvwprintw(windows[4], 8,  2, "QUANTIZE");
                mvwprintw(windows[4], 9,  2, "%i", opz.getActiveTrackParameters().quantize);
                mvwprintw(windows[4], 11, 2, "PORTAMENTO");
                mvwprintw(windows[4], 12, 2, "%i", opz.getActivePageParameters().portamento );
            }

            for (size_t i = 0; i < 5; i++)
                wrefresh(windows[i]);

            change = false;
            change_data = false;
        }

    }
    
    waitForKeys.join();
    endwin();
    opz.disconnect();

    exit(0);
}
