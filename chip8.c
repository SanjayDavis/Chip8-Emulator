#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

typedef struct{
    SDL_Window * window;
    SDL_Renderer * renderer;
    SDL_AudioSpec want,have;
    SDL_AudioDeviceID dev;
} sdl_t;

typedef enum {    // Emulator States
    QUIT,
    RUNNING,
    PAUSE,
} state_t;

// chip8-extension

typedef enum {
    CHIP8,
    SUPERCHIP,
    XOCHIP
} extension_t;

typedef struct{
    uint32_t window_height;
    uint32_t window_width;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_factor; //  amount to scale a pixel to 20x
    bool pixel_outlines;
    uint32_t inst_per_second; // instructions per second  (clock rate )
    uint32_t square_wave_freq;  // frequency of square sound
    uint32_t audio_sample_rate; 
    uint16_t volume; // how loud or not is the sound
    extension_t current_extension; // current quirks/extension support
} config_t;


typedef struct{
    uint16_t opcode;
    uint16_t NNN; // 12bit address
    uint8_t NN;   //8 bit constant
    uint8_t N;     // 4 bit constant
    uint8_t X;      // register
    uint8_t Y;      // register
} instruction_t;


typedef struct{
    uint8_t ram[4096];
    bool display[64*32];  // for DXYN  display [x y]
    state_t state;
    uint16_t stack[12]; // for tweleve nesting states 
    uint16_t *stack_ptr;
    uint8_t V[16]; // for all registers
    uint16_t I; //index register for the address 
    uint16_t PC; // program counter
    uint8_t delay_timer; // Decrements at 60hz when >0
    uint8_t sound_timer;  // Decrements at 60hz and plays tone when > 0
    bool keypads[16]; //keypads
    char * rom_name; // rom name
    instruction_t  inst;
} chip8_t;


void audio_callback(void * userdata , uint8_t * stream , int len){
    config_t * config = (config_t *) userdata;

    int16_t * audio_data = (int16_t * ) stream;
    static uint32_t running_sample_index = 0;
    const int32_t square_wave_period = config->audio_sample_rate / config->square_wave_freq;
    const int32_t half_square_wave_period = square_wave_period / 2;

    // length is in bytes so divide by 2
    for(int i = 0;i < len/2 ; i++){
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ?
         config->volume : -config->volume ;
    }

}

bool initSDl(sdl_t *sdl,config_t *config){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER ) != 0){
        SDL_Log("COuld not initialize SDL subsystems %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP 8 Emulator",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    config->window_width * config->scale_factor,
                                    config->window_height * config->scale_factor,
                                    0);
    if(!sdl->window){
        SDL_Log("SDL_CreateWindow Error : %s",SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("The CreateRenderer did not work : %s",SDL_GetError());
        return false;
    }


    // AUDIO
    sdl->want = (SDL_AudioSpec){
        .freq = 44100,   // CD quality
        .format = AUDIO_S16LSB, // little endian
        .channels = 1,  // mono , 1 channel
        .samples = 512,
        .callback = audio_callback,
        .userdata = config, // userdata passed to audio callback


    };

    sdl->dev = SDL_OpenAudioDevice(NULL,0, &sdl->want, &sdl->have, 0);
    
    if(sdl->dev == 0){
        SDL_Log("Could not get an audio device .\n");
        return false;
    } 

    if(sdl->want.format != sdl->have.format ||
        sdl->want.channels != sdl->have.channels) {
            SDL_Log("Could not get desired Audio Spec\n");
        }  

    SDL_PauseAudioDevice(sdl->dev, 0); // Start playing audio


    return true;
}

bool init_chip8(chip8_t * chip8, char rom_name []){
    const uint32_t starting_point = 0x200;
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    //load font

    memcpy(&chip8->ram[0],font,sizeof(font));
    //load rom

    FILE * rom = fopen(rom_name,"rb");
    if(!rom){
        SDL_Log("Rom dosen't exist");
        return false;
    }
    fseek(rom,0,SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof(chip8->ram) - starting_point;
    rewind(rom);

    if(rom_size > max_size){
        SDL_Log("Rom size is too big ...");
        return false;
    }

    if(fread(&chip8->ram[starting_point],rom_size,1,rom) != 1){
        SDL_Log("Cannot read into the rom file ..");
        return false;
    }

    fclose(rom);
    

    chip8->state = RUNNING;
    chip8->PC = starting_point;
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];
    return true;
}

void cleanupSDL(const sdl_t * sdl){
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_CloseAudioDevice(sdl->dev);
    SDL_Quit(); // shutdown the SDL
}

//set up initial emulator configuration
bool set_config_args(config_t * config,int argc, char ** argv){
    // set defaults
    config -> window_height = 32;    // chip-8 original resolution
    config -> window_width = 64;  
    config->fg_color = 0xFFFFFFFF; // white
    config->bg_color = 0x00000000; // black
    config->scale_factor = 20;
    config->inst_per_second = 700;
    config->square_wave_freq = 440;
    config->volume = 2500; // max = 320000 
    config->audio_sample_rate = 44100; // cd quality
    config->current_extension = CHIP8;
    for(int i=1 ; i < argc ;i++){
        (void)argv[i];  // prevernt compiler error
    }
    config->pixel_outlines = true;
    //override defaults fom arguments(later)
    return true;
}

void clearScreen(sdl_t * sdl ,config_t  * config){
    uint8_t r = (config->bg_color >> 24) & 0xFF ;
    uint8_t g = (config->bg_color >> 16) & 0xFF ;
    uint8_t b = (config->bg_color >>  8) & 0xFF ;
    uint8_t a = (config->bg_color >>  0) & 0xFF ;


    SDL_SetRenderDrawColor(sdl->renderer,r,g,b,a);
    SDL_RenderClear(sdl->renderer);
}

void updateScreen(sdl_t  * sdl , config_t * config , chip8_t * chip8){
    SDL_Rect rect = {.x = 0, .y = 0 , .w = config->scale_factor , .h = config->scale_factor};
    
    uint8_t fg_r = (config->fg_color >> 24) & 0xFF ;
    uint8_t fg_g = (config->fg_color >> 16) & 0xFF ;
    uint8_t fg_b = (config->fg_color >>  8) & 0xFF ;
    uint8_t fg_a = (config->fg_color >>  0) & 0xFF ;

    uint8_t bg_r = (config->bg_color >> 24) & 0xFF ;
    uint8_t bg_g = (config->bg_color >> 16) & 0xFF ;
    uint8_t bg_b = (config->bg_color >>  8) & 0xFF ;
    uint8_t bg_a = (config->bg_color >>  0) & 0xFF ;


    for(uint32_t i = 0;i<sizeof(chip8->display);i++){
        rect.x = (i % config->window_width) * config->scale_factor;
        rect.y = (i / config->window_width) * config->scale_factor;


        if(chip8->display[i]){
            // pixel is on FG
            SDL_SetRenderDrawColor(sdl->renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);

            // if user requested pixel config
            if(config->pixel_outlines == true){
                SDL_SetRenderDrawColor(sdl->renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl->renderer, &rect);
            }
            
        }
        else {
            // pixel is off BG
            SDL_SetRenderDrawColor(sdl->renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl->renderer, &rect);
        }
    }
    
    SDL_RenderPresent(sdl->renderer);
}

// chip8 Keypad

void handle_input(chip8_t * chip8, config_t * config){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT:
                chip8->state = QUIT;
                return;
            case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym){
                    
                    case SDLK_ESCAPE: 
                        chip8->state = QUIT;
                        return;

                    case  SDLK_SPACE:
                        //spacebar
                        if(chip8->state == RUNNING){
                            chip8->state = PAUSE;
                            puts("=====PAUSED=====");
                        } 
                        else {
                            chip8->state = RUNNING;
                            puts("====RUNNNING=====");
                        }
                        break;
                    
                    
                    // map qwerty to chip8 keypad
                    case SDLK_1: chip8->keypads[0x1] = true; break;
                    case SDLK_2: chip8->keypads[0x2] = true; break;
                    case SDLK_3: chip8->keypads[0x3] = true; break;
                    case SDLK_4: chip8->keypads[0xC] = true; break;

                    case SDLK_q: chip8->keypads[0x4] = true; break;
                    case SDLK_w: chip8->keypads[0x5] = true; break;
                    case SDLK_e: chip8->keypads[0x6] = true; break;
                    case SDLK_r: chip8->keypads[0xD] = true; break;

                    case SDLK_a: chip8->keypads[0x7] = true; break;
                    case SDLK_s: chip8->keypads[0x8] = true; break;
                    case SDLK_d: chip8->keypads[0x9] = true; break;
                    case SDLK_f: chip8->keypads[0xE] = true; break;

                    case SDLK_z: chip8->keypads[0xA] = true; break;
                    case SDLK_x: chip8->keypads[0x0] = true; break;
                    case SDLK_c: chip8->keypads[0xB] = true; break;
                    case SDLK_v: chip8->keypads[0xF] = true; break;


                    case SDLK_UP:
                        if(config->volume > 7500) config->volume = 8000;
                        else config->volume +=500;
                        break;

                    case SDLK_DOWN:
                        if(config->volume < 500 )config->volume = 0;
                        else config->volume -=500; 



                    default:
                        break;
                }
                break;
            }
                
            case SDL_KEYUP:
            {
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8->keypads[0x1] = false; break;
                    case SDLK_2: chip8->keypads[0x2] = false; break;
                    case SDLK_3: chip8->keypads[0x3] = false; break;
                    case SDLK_4: chip8->keypads[0xC] = false; break;
                    case SDLK_q: chip8->keypads[0x4] = false; break;
                    case SDLK_w: chip8->keypads[0x5] = false; break;
                    case SDLK_e: chip8->keypads[0x6] = false; break;
                    case SDLK_r: chip8->keypads[0xD] = false; break;
                    case SDLK_a: chip8->keypads[0x7] = false; break;
                    case SDLK_s: chip8->keypads[0x8] = false; break;
                    case SDLK_d: chip8->keypads[0x9] = false; break;
                    case SDLK_f: chip8->keypads[0xE] = false; break;
                    case SDLK_z: chip8->keypads[0xA] = false; break;
                    case SDLK_x: chip8->keypads[0x0] = false; break;
                    case SDLK_c: chip8->keypads[0xB] = false; break;
                    case SDLK_v: chip8->keypads[0xF] = false; break;
                }
                break;

            }
            break;
            
            
            default:
                break;   
        }
    }
}

//emulate chip8 instruction
void emulate_instruction(chip8_t * chip8, config_t config){
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1];
    chip8->PC +=2; // increment for next opcode

    // fill out instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

    switch((chip8->inst.opcode >> 12) & 0x0F){
        case 0x0:
            if(chip8->inst.NN == 0xE0){
                memset(&chip8->display[0],false,sizeof(chip8->display));
            } else if (chip8->inst.NN == 0xEE){
                // return from subroutine
                chip8->PC = *--chip8->stack_ptr;
            } else {
                //unimplimented opcode  calling for machine code 
            }
            break;

        case 0x01:
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02:
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03:
            {
                // if vx == nn then skip next instruction
                if(chip8->V[chip8->inst.X] == chip8->inst.NN) chip8->PC +=2;
            }
            break;

        case 0x04:
            {
                // if vx == nn then skip next instruction
                if(chip8->V[chip8->inst.X] != chip8->inst.NN) chip8->PC +=2;
            }
            break;

        case 0x05:
            if(chip8->inst.N != 0) break;// wrong unimplimented rom opcode
            if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) chip8->PC +=2;
            break;
        
        case 0x06:
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08:{
            switch (chip8->inst.N) {
                case 0:
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    break;

                case 1:
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    if (config.current_extension == CHIP8) chip8->V[0xF] = 0; // chip8 only quirk
                    break;

                case 2:
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    if (config.current_extension == CHIP8) chip8->V[0xF] = 0;
                    break;

                case 3:
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    if (config.current_extension == CHIP8) chip8->V[0xF] = 0;
                    break;

                case 4:
                {
                    const bool carry = ((uint16_t) (chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                }
                    
                    break;

                case 5:
                {
                    const bool carry = (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]);
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                }
                    break;

                case 6:
                {
                    bool carry;
                    
                    if(config.current_extension == CHIP8){
                        carry = chip8->V[chip8->inst.Y] & 1;
                        chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;
                    }
                    else {
                        carry = chip8->V[chip8->inst.X] & 1;
                        chip8->V[chip8->inst.X] >>=1;

                    }
                    chip8->V[0xF] = carry;
                }
                    break;

                case 7:
                {
                    const bool carry = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]);
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = carry;
                }
                    break;
                
                case 0xE:
                {
                    bool carry;
                    if(config.current_extension == CHIP8){
                        carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;
                        chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1;
                    }
                    else {
                        carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;
                        chip8->V[chip8->inst.X] <<=1;
                    }
                    chip8->V[0xF] = carry;
                }
                    break;


                default:
                    break;
                
            }
            
        }
        break;

        case 0x09:
            if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
            chip8->PC +=2;
            break;

        case 0x0A:
            chip8->I = chip8->inst.NNN;
            break;

        case 0x0B:
            chip8->PC = chip8->V[0] + chip8->inst.NNN;
            break;
        
        case 0x0C:
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;
  
        case 0x0D:
        {
            uint8_t X = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y = chip8->V[chip8->inst.Y] % config.window_height; // Fix: Use inst.Y instead of inst.X
            chip8->V[0xF] = 0;
            uint8_t original_X = X;

            for(uint8_t i = 0; i < chip8->inst.N; i++) {
                uint8_t sprite_data = chip8->ram[chip8->I + i];
                X = original_X;

                for(int j = 7; j >= 0; j--) {
                    bool *pixel = &chip8->display[Y * config.window_width + X];
                    bool sprite_bit = (sprite_data & (1 << j));

                    if(sprite_bit && *pixel) {
                        chip8->V[0xF] = 1; // Set collision flag
                    }

                    *pixel ^= sprite_bit; // XOR pixel with sprite bit

                    if(++X >= config.window_width) break;
                }

                if(++Y >= config.window_height) break;
            }
        }
        break;

        case 0x0E:
            if(chip8->inst.NN == 0x9E) {
                if (chip8->keypads[chip8->V[chip8->inst.X]])
                chip8->PC +=2;
            } 
            else if (chip8->inst.NN == 0xA1) {
                if (!chip8->keypads[chip8->V[chip8->inst.X]])
                chip8->PC +=2;
            }
            break;

        case 0x0F:
            {
                switch(chip8->inst.NN){
                    case 0x0A: {
                        static bool any_key_pressed = false;
                        static uint8_t key = 0xFF;

                        for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypads; i++) 
                            if (chip8->keypads[i]) {
                                key = i;    
                                any_key_pressed = true;
                                break;
                            }

                        if (!any_key_pressed) chip8->PC -= 2; 
                        else {
                            if (chip8->keypads[key])     
                                chip8->PC -= 2;
                            else {
                                chip8->V[chip8->inst.X] = key;     
                                key = 0xFF;                       
                                any_key_pressed = false;          
                            }
                        }
                        break;
                    }
                    case 0x1E:
                        chip8->I += chip8->V[chip8->inst.X];
                        break;
                    
                    case 0x07:
                        chip8->V[chip8->inst.X] = chip8->delay_timer;
                        break;

                    
                    case 0x15:
                        chip8->delay_timer = chip8->V[chip8->inst.X];
                        break;

                    case 0x18:
                        chip8->sound_timer = chip8->V[chip8->inst.X];
                        break;

                    case 0x29:
                        chip8->I = chip8->V[chip8->inst.X] *5;
                        break;

                    case 0x33:
                    {
                        uint8_t bcd = chip8->V[chip8->inst.X];
                        chip8->ram[chip8->I+2] = bcd % 10; bcd /=10;
                        chip8->ram[chip8->I+1] = bcd % 10; bcd /=10;
                        chip8->ram[chip8->I] = bcd;
                        break;
                    }
                    case 0x55:
                        for(uint8_t i =0 ; i<= chip8->inst.X; i++){
                            if(config.current_extension == CHIP8){
                                chip8->ram[chip8->I++] = chip8->V[i]; 
                            }
                            else chip8->ram[chip8->I + i] = chip8->V[i]; 
                        }
                        break;

                    case 0x65:
                        for(uint8_t i =0 ; i<= chip8->inst.X; i++){
                            if(config.current_extension == CHIP8) chip8->V[i] = chip8->ram[chip8->I ++];
                            else  chip8->V[i] = chip8->ram[chip8->I + i] ; 

                        }

                        break;
                        
                    default:
                        break;
                }
            }
                    

        default:{
            break; // unimplimented opcode
        }
    }

}

void update_timers(sdl_t sdl,chip8_t *chip8){
    if(chip8->delay_timer > 0) chip8->delay_timer--;
    if(chip8->sound_timer > 0){
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl.dev,0);
    }
    else {
        SDL_PauseAudioDevice(sdl.dev,1);
        // dont play sound
    }
        
}


int main(int argc, char ** argv){

    if(argc < 2){
        fprintf(stderr,"Usage %s <rom_name>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    // initialize the sdl
    config_t config = {0};
    sdl_t sdl = {0};
    
    if(!set_config_args(&config, argc, argv)) exit(EXIT_FAILURE);
    if(!initSDl(&sdl,&config)) exit(EXIT_FAILURE);

    // initialize the machine
    chip8_t chip8 = {0};
    char * rom_name = argv[1];

    if(!init_chip8(&chip8,rom_name)) exit(EXIT_FAILURE);
    // initial screen clear 
    clearScreen(&sdl, &config);
    srand(time(NULL));


    while (chip8.state != QUIT) {


        handle_input(&chip8,&config);
        if(chip8.state == PAUSE) continue;

        uint64_t before_frame = SDL_GetPerformanceCounter();

        for(uint32_t i =0  ; i<config.inst_per_second / 60;i++)  emulate_instruction(&chip8,config);
 

        uint64_t after_frame = SDL_GetPerformanceCounter();

        const double time_elapsed = (double)((after_frame - before_frame) * 1000) / SDL_GetPerformanceFrequency(); 

        //clearScreen(&sdl, &config); // Optional — keep or remove depending on rendering logic
        SDL_Delay(16.67f > time_elapsed ? 16.67 - time_elapsed : 0); // ~60 fps
        // Emulate chip8 instructions here

        updateScreen(&sdl,&config,&chip8);

        update_timers(sdl,&chip8); // update delay and sound timers
        
    }


    //final cleanup
    cleanupSDL(&sdl);
    
}
