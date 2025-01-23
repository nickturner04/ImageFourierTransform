#include <array>
#include <complex>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
using namespace std;

#define FILENAME "resources/cat.png"
#define KERNELNAME "resources/kernel/gaussian.png"
#define SIZE 512
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 1024
#define SIZE2 SIZE*SIZE
#define ROWVEC vector<complex<float>>
#define IMGARRAY array<complex<float>,SIZE2>
#define E_COMPLEX complex<float>(M_Ef32, 0)

TTF_Font *gFont;

struct Label {
    SDL_Texture *texture;
    int charSize;
    int length;
    int x;
    int y;

    void draw(SDL_Renderer * renderer ) {
        SDL_Rect rect = {x,y,charSize*length,charSize};
        SDL_RenderCopy(renderer,texture,nullptr,&rect);
    }

    static Label create(SDL_Renderer * renderer ,const char *text, int x, int y) {
        auto surf = TTF_RenderText_Solid(gFont,text,{255,0,0,255});
        auto texture = SDL_CreateTextureFromSurface(renderer, surf);
        return {texture,16,static_cast<int>(strlen(text)),x,y};
    }
};

struct Labels {
    SDL_Renderer *renderer;
    vector<Label> * labels;

    explicit Labels(SDL_Renderer * renderer) {
        this->renderer = renderer;
        labels = new vector<Label>;
    }

    void add(const char *text,int x,int y) const {
        labels->push_back(Label::create(renderer,text,x,y));
    }
    void draw() const {
        for (auto & label : *labels) {
            label.draw(renderer);
        }
    }
};


struct BarChart {
    SDL_FPoint position;
    float height;
    float barWidth;
    SDL_FRect displayRect;
    SDL_Texture * texture = nullptr;

    void draw(SDL_Renderer* renderer, const array<complex<float>,SIZE2> * data, float scale = 1.f) {
        const auto n = data->size();
        //Draw Background
        const SDL_FRect rectangle = {position.x,position.y,barWidth * n,height};
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderFillRectF(renderer,&rectangle);

        //Draw Each data point
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        for (auto i = 0; i < n; i++) {
            //get the abs of the complex number and clip to fit the chart
            const float x = min(fabs(data->at(i)) * scale,1.2f);
            SDL_FRect bar = {position.x + barWidth * i,position.y,barWidth,height * x};
            SDL_RenderFillRectF(renderer,&bar);
        }
    }
    void cacheTexture(SDL_Renderer * renderer, const IMGARRAY * data, const float scale = 1.f){
        const auto n = static_cast<int>(data->size());
        displayRect = { position.x,position.y,WINDOW_WIDTH,height};
        if(texture == nullptr) {
            texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,static_cast<int>(barWidth) * WINDOW_WIDTH,static_cast<int>(height));
        }
        SDL_SetRenderTarget(renderer,texture);
        //Fill Texture Black
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderFillRectF(renderer,nullptr);

        //Draw Data

        for (int i = 0; i < n; ++i) {
            SDL_SetRenderDrawColor(renderer,255,255,255,255);
            float x = fabs(data->at(i)) * scale;
            if(x > 1.f) {
                SDL_SetRenderDrawColor(renderer,255,0,0,255);
                x = 1.f;
            }
            SDL_FRect bar = {barWidth * i,0,barWidth,height * x};
            SDL_RenderFillRectF(renderer,&bar);
        }
        SDL_SetRenderTarget(renderer,nullptr);
    }
    void drawCached(SDL_Renderer * renderer) const {
        SDL_RenderCopyF(renderer,texture,nullptr,&displayRect);
    }
};


void transformRow(array<complex<float>,SIZE2> * pixels, const int y, const bool invert = false) {
    const auto e_complex = complex<float>(M_E, 0);
    complex<float> i_complex = - complex<float>(0, 1);
    auto scale = 1.f;

    //Make a new array to hold the unchanged row
    auto X =  array<complex<float>, SIZE> ();
    const auto start = pixels->data() + y * SIZE;
    const auto end = start + SIZE;

    //Copy the row to this new array
    copy(start, end, X.begin());
    constexpr auto N = SIZE;

    //Inverse Transform
    if(invert) {
        i_complex = complex<float>(0, 1);
        scale = 1.f / static_cast<float>(N);
    }

    //Use the data from the copied row to modify the original row in place
    for (int k = 0; k < N ; ++k) {
        complex<float> total = complex<float>(0, 0);
        for (int n = 0; n < N; ++n) {

            auto Xn = X.at(n);
            auto i2pi = i_complex * 2.f * M_PIf32;
            auto kOverN = static_cast<float>(k) / static_cast<float>(N);
            total += Xn * pow(e_complex, i2pi * kOverN * static_cast<float>(n));

        }
        pixels->at(SIZE * y + k) = total * scale;
    }
}

ROWVEC fftRow(ROWVEC row) {

    const auto n = row.size();
    if(n == 1) return row;
    const auto half = n / 2;
    complex<float> i_complex = - complex<float>(0, 1);


    const complex<float> o = pow(E_COMPLEX, (2.f * M_PIf32 * i_complex) / static_cast<float>(n));

    auto even =  vector<complex<float>>();
    auto odd =  vector<complex<float>>();

    even.reserve(half);
    odd.reserve(half);

    for (int i = 0; i < n; i+=2) {
        even.push_back(row[i]);
        odd.push_back(row[i+1]);
    }

    const auto transformOdd = fftRow(odd);
    const auto transformEven = fftRow(even);

    auto transform = ROWVEC(n);

    for (int i = 0; i < half; ++i) {
        transform[i] = (transformEven[i] + pow(o,i) * transformOdd[i]);
        transform[i + half] = (transformEven[i] - pow(o,i) * transformOdd[i]);
    }

    return transform;
}

ROWVEC fftiRow(ROWVEC row) {

    const auto n = row.size();
    if(n == 1) return row;
    const auto half = n / 2;
    complex<float> i_complex = complex<float>(0, 1);
    const auto scale = 1.f / static_cast<float>(n);


    const complex<float> o = pow(E_COMPLEX, (2.f * M_PIf32 * i_complex) / static_cast<float>(n));

    auto even =  vector<complex<float>>();
    auto odd =  vector<complex<float>>();

    even.reserve(half);
    odd.reserve(half);

    for (int i = 0; i < n; i+=2) {
        even.push_back(row[i]);
        odd.push_back(row[i+1]);
    }

    const auto transformOdd = fftiRow(odd);
    const auto transformEven = fftiRow(even);

    auto transform = ROWVEC(n);

    for (int i = 0; i < half; ++i) {
        transform[i] = (transformEven[i] + pow(o,i) * transformOdd[i]);
        transform[i + half] = (transformEven[i] - pow(o,i) * transformOdd[i]);
    }

    return transform;
}

void fft(IMGARRAY * data, const bool invert = false) {
    for (int i = 0; i < SIZE; ++i) {
        const auto start = data->data() + i * SIZE;
        ROWVEC row(start,start + SIZE);

        ROWVEC tRow;
        if(invert) {
            tRow = fftiRow(row);
            for (auto & j : tRow) {
                j *= 1.f/ static_cast<float>(SIZE);
            }
        }
        else {
            tRow = fftRow(row);
        }

        copy(tRow.data(),tRow.data() + SIZE,data->data() + i * SIZE);

        cout << "Transformed Row: " << i << "/" << SIZE << "\n";
    }
}

void transformData(array<complex<float>,SIZE2> * data, const bool invert = false) {
    for (int i = 0; i < SIZE; ++i) {
        transformRow(data,i,invert);
        cout << "Transformed Row: " << i << "/" << SIZE << "\n";
    }
}

void rowsToColumns(IMGARRAY * data) {
    const IMGARRAY temp = *data;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            data->at(i * SIZE + j) = temp[j * SIZE + i];
        }
    }
}

void fullTransform(IMGARRAY * data, const bool invert = false) {
    fft(data, invert);
    rowsToColumns(data);
    fft(data, invert);
}

float colorToFloat(const Uint8 color) {
    return static_cast<float>(color) / 255.f;
}

uint8_t floatToColor(float color) {
    return static_cast<uint8_t>(255.f * min(color, 1.f));
}

void surfaceToArray(const SDL_Surface * surface, IMGARRAY * data) {
    const auto n = data->size();
    for (size_t i = 0; i < n; ++i) {
        void * p = surface->pixels + i * sizeof(Uint32);
        const auto *pixel = static_cast<Uint32 *>(p);
        //Cast the void pointer to a Uint32 pointer to avoid throwing a compiler error


        Uint8 r, g, b;
        SDL_GetRGB(*pixel ,surface->format, &r, &g, &b);
        //Discard green and blue as the image is greyscale
        data->at(i) = complex<float>(colorToFloat(r), 0);
    }
}

void setPixel(const SDL_Surface * surface, const int i, const float rawColor) {
    const auto pixel = static_cast<Uint32 *>(surface->pixels + i * surface->format->BytesPerPixel);
    const Uint8 color = floatToColor(rawColor);

    *pixel = SDL_MapRGB(surface->format,color,color,color);
}

void setPixel(const SDL_Surface * surface, const int x, const int y, const float rawColor) {
    const auto pixel = static_cast<Uint32 *>(surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel);
    const Uint8 color = floatToColor(rawColor);

    *pixel = SDL_MapRGB(surface->format,color,color,color);
}

float logBase(float x, float base) {
    return logf(x) / logf(base);
}

SDL_Surface * arrayToSurface(const array<complex<float>,SIZE2> * data, const float scale = 1.f) {
    SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(0, SIZE, SIZE, 32, SDL_PIXELFORMAT_ARGB8888);
    for (int i = 0; i < SIZE2; ++i) {
        const float rawColor = fabs(data->at(i)) * scale;
        setPixel(surface,i,rawColor);
    }
    return surface;

}
//remap a value to [0,1] space
float norm(const float x, const float l, const float h) {
    return (x - l) / (h - l);
}
SDL_Surface * arrayToSurfaceNorm(const array<complex<float>,SIZE2> * data) {
    //Find Upper and Lower Bound
    float hi = FLT_MIN;
    float lo = FLT_MAX;
    for (int i = 0; i < SIZE2; ++i) {
        const float rawColor = fabs(data->at(i));
        if(rawColor > hi) hi = rawColor;
        if(rawColor < lo) lo = rawColor;

    }


    SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(0, SIZE, SIZE, 32, SDL_PIXELFORMAT_ARGB8888);
    for (int i = 0; i < SIZE2; ++i) {
        const float rawColor = fabs(data->at(i));
        //Set pixel to the remapped value
        setPixel(surface,i,1 - norm(rawColor,hi,lo));
    }
    return surface;
}

inline float getRawColor(const IMGARRAY * src, const int x, const int y) {
    return fabs(src->at(y * SIZE + x));
}

SDL_Surface * fftShift(const IMGARRAY * src, const float scale = 1.f) {
    SDL_Surface * surface = SDL_CreateRGBSurfaceWithFormat(0, SIZE, SIZE, 32, SDL_PIXELFORMAT_ARGB8888);
    constexpr auto l = SIZE / 2;
    //first corner
    for (int i = 0; i < l; ++i) {
        for (int j = 0; j < l; ++j) {
            const float col = getRawColor(src, i + l, j + l) * scale;
            setPixel(surface,i,j,col);
            //dst->at(j * SIZE + i) = src->at((j + l ) * SIZE + (i + l ));
        }
    }
    //Second corner
    for (int i = l; i < SIZE; ++i) {
        for (int j = 0; j < l; ++j) {
            const float col = getRawColor(src, i + l, j + l - 1) * scale;
            setPixel(surface,i,j,col);
            //dst->at(j * SIZE + i) = src->at((j + l - 1) * SIZE + (i + l ));
        }
    }
    //Third corner
    for (int i = 0; i < l; ++i) {
        for (int j = l; j < SIZE; ++j) {
            const float col = getRawColor(src, i + l, j - l) * scale;
            setPixel(surface,i,j,col);
            //dst->at(j * SIZE + i) = src->at((j - l) * SIZE + (i + l));
        }
    }
    //Fourth corner
    for (int i = l; i < SIZE; ++i) {
        for (int j = l; j < SIZE; ++j) {
            const float col = getRawColor(src, i - l, j - l) * scale;
            setPixel(surface,i,j,col);
            //dst->at(j * SIZE + i) = src->at((j - l) * SIZE + (i - l));
        }
    }

    return surface;
}

void convolve(IMGARRAY * data ) {
    SDL_Surface * kernelImage = IMG_Load(KERNELNAME);
    const SDL_Surface * converted = SDL_ConvertSurfaceFormat(kernelImage, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(kernelImage);
    IMGARRAY kernel;
    surfaceToArray(converted,&kernel);
    fullTransform(&kernel);

    for (int i = 0; i < SIZE2; ++i) {
        constexpr auto correction = 0.075f;
        data->at(i) *= kernel[i] * correction;
    }
}


int main()
{
    std::cout << "Hello, World!\n";
    SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();


    gFont = TTF_OpenFont("resources/sampleFont.ttf", 32);

    BarChart barChart = {0,512,64,1};
    BarChart barChart2 = {0,512 + 128,64,1};
    BarChart barChart3 = {0,512 + 256,64,1};

    SDL_Window * window = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    const auto * labels = new Labels(renderer);

    SDL_Surface * sampleImage = IMG_Load(FILENAME);
    SDL_Surface * converted = SDL_ConvertSurfaceFormat(sampleImage, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(sampleImage);
    //turn surface into a texture in order to display it on screen
    SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer, converted);

    IMGARRAY transformedArray;
    surfaceToArray(converted,&transformedArray);
    barChart.cacheTexture(renderer,&transformedArray);
    SDL_FreeSurface(converted);
    fullTransform(&transformedArray);
    //convolve(&transformedArray);


    float scale = 0.01f;
    barChart2.cacheTexture(renderer,&transformedArray,scale);
    SDL_Surface * transformedSurface = fftShift(&transformedArray,scale);
    SDL_Texture * transformedTexture = SDL_CreateTextureFromSurface(renderer, transformedSurface);


    SDL_Rect screen1 = {0,0,512,512};
    SDL_Rect screen2 = {512,0,512,512};



    labels->add("Raw Image",0,0);
    labels->add("Transformed Image",512,0);
    labels->add("Bar Chart:(Raw Image)",0,512);
    labels->add("Controls:",0,512 + 256);
    labels->add("S: Enter Scale Factor",0,512+256 + 16);
    labels->add("I: Inverse Fourier Transform",0,512 + 256 + 32);
    labels->add("C: Convolution Blur",0,512 + 256 + 48);

    bool running =true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                IMG_SavePNG(transformedSurface, "output/output.png");
                Uint32 formatEnum = SDL_GetWindowPixelFormat(window);
                SDL_Surface * screenShot = SDL_CreateRGBSurfaceWithFormat(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, formatEnum);
                SDL_RenderReadPixels(renderer,NULL,formatEnum,screenShot->pixels,screenShot->pitch);
                IMG_SavePNG(screenShot, "output/screenshot.png");
            }
            else if(event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    default: break;
                    case SDLK_s:
                        cout << "Enter Scale for Display: " << endl;
                        cin >> scale;
                        SDL_FreeSurface(transformedSurface);
                        transformedSurface = arrayToSurface(&transformedArray, scale);
                        SDL_DestroyTexture(transformedTexture);
                        transformedTexture = SDL_CreateTextureFromSurface(renderer, transformedSurface);
                        break;
                    case SDLK_i:
                        cout << "Inverting Transformed image" << endl;
                        fft(&transformedArray,true);
                        rowsToColumns(&transformedArray);
                        fft(&transformedArray,true);
                        SDL_FreeSurface(transformedSurface);
                        transformedSurface = arrayToSurfaceNorm(&transformedArray);
                        SDL_DestroyTexture(transformedTexture);
                        transformedTexture = SDL_CreateTextureFromSurface(renderer, transformedSurface);
                        break;
                    case SDLK_c:
                        cout << "Performing Convolution" << endl;
                        convolve(&transformedArray);
                        SDL_FreeSurface(transformedSurface);
                        transformedSurface = fftShift(&transformedArray, scale);
                        SDL_DestroyTexture(transformedTexture);
                        transformedTexture = SDL_CreateTextureFromSurface(renderer, transformedSurface);
                }
            }
        }
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, NULL);
        SDL_RenderCopy(renderer,texture,NULL,&screen1);
        SDL_RenderCopy(renderer,transformedTexture,NULL,&screen2);

        barChart.drawCached(renderer);
        barChart2.drawCached(renderer);
        //barChart3.draw(renderer,&columns);

        labels->draw();
        SDL_RenderPresent(renderer);
    }
    IMG_Quit();
    SDL_Quit();

    return 0;
}