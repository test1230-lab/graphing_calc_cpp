#include <iostream>
#include <unordered_map>
#include <string_view>
#include <string>
#include <vector>
#include <utility>
#include <stack>
#include <regex>
#include <variant>
#include <cmath>
#include <SDL.h>
#undef main

constexpr uint32_t white = 0xFFFFFFFF;
constexpr uint32_t black = 0x00000000;
constexpr uint32_t green = 0xFF007F00;
constexpr uint32_t red = 0xFFBB0000;
constexpr uint32_t yellow = 0xFFFFFF00;
constexpr uint32_t bright_red = 0xFFFF0000;
constexpr uint32_t blue = 0xFF0000FF;

constexpr int screen_w = 640;
constexpr int screen_h = 640;
constexpr int grid_spacing = 32;
constexpr const char* win_title = "Graphing Calc";
constexpr const char* var_name = "x";

struct pt_2d
{
    int x, y;
};

namespace parser
{
    constexpr int LEFT_ASSOC = 0;
    constexpr int RIGHT_ASSOC = 1;

    //pair is <prec, assoc_id>
    static const std::unordered_map<std::string, std::pair<int, int>> assoc_prec{ {"^", std::make_pair(4, RIGHT_ASSOC)},
                                                                                  {"*", std::make_pair(3, LEFT_ASSOC)},
                                                                                  {"/", std::make_pair(3, LEFT_ASSOC)},
                                                                                  {"+", std::make_pair(2, LEFT_ASSOC)},
                                                                                  {"-", std::make_pair(2, LEFT_ASSOC)} };

    static const std::unordered_map<std::string, double(*)(double)> unary_func_tbl{ {"sin", &sin},
                                                                                    {"cos", &cos},
                                                                                    {"sqrt", &sqrt},
                                                                                    {"abs", &abs},
                                                                                    {"tan", &tan},
                                                                                    {"acos", &acos},
                                                                                    {"asin", &asin},
                                                                                    {"atan", &atan},
                                                                                    {"log", &log},
                                                                                    {"log10", &log10},
                                                                                    {"cosh", &cosh},
                                                                                    {"sinh", &sinh},
                                                                                    {"tanh", &tanh},
                                                                                    {"exp", &exp},
                                                                                    {"cbrt", &cbrt},
                                                                                    {"tgamma", &tgamma},
                                                                                    {"lgamma", &lgamma},
                                                                                    {"ceil", &ceil},
                                                                                    {"floor", &floor},
                                                                                    {"acosh", &acosh},
                                                                                    {"asinh", &asinh},
                                                                                    {"atanh", &atanh} };

    bool is_right_assoc(const std::string& str)
    {
        int id = assoc_prec.at(str).second;
        if (id == 1) return true;
        else return false;
    }

    bool is_left_assoc(const std::string& str)
    {
        int id = assoc_prec.at(str).second;
        if (id == 0) return true;
        else return false;
    }

    bool is_binary_op(std::string_view str)
    {
        if (str == "%" || str == "/" || str == "*" || str == "+" || str == "-" || str == "^")
        {
            return true;
        }
        else return false;
    }

    bool is_func(std::string_view str)
    {
        if (str == "sin" || str == "tan" || str == "acos" || str == "asin" || str == "abs" || \
            str == "atan" || str == "cosh" || str == "sinh" || str == "cos" || str == "tanh" || \
            str == "acosh" || str == "asinh" || str == "atanh" || str == "exp" || str == "ldexp" || \
            str == "log" || str == "log10" || str == "sqrt" || str == "cbrt" || str == "tgamma" || \
            str == "lgamma" || str == "ceil" || str == "floor")
        {
            return true;
        }
        else return false;
    }


    //handls decimal numbers
    bool is_num(std::string_view str)
    {
        int num_found_periods = 0;
        for (const auto c : str)
        {
            if (c == '.')
            {
                num_found_periods++;
                if (num_found_periods > 1)
                {
                    return false;
                }
            }
            if (!isdigit(c) && c != '.')
            {
                return false;
            }
        }
        return true;
    }

    int get_prec(const std::string& str)
    {
        if (is_func(str))
        {
            return 1; //TODO: check it this is the correct value
        }
        else if (is_binary_op(str))
        {
            return assoc_prec.at(str).first;
        }
        else
        {
            return 0;
        }
    }

    
    //from https://stackoverflow.com/a/56204256
    //modified regex expr
    std::vector<std::string> tokenize(const std::string& str)
    {
        std::vector<std::string> res;
        const std::regex words_regex("(sin|tan|acos|asin|abs|atan|cosh|sinh|cos|"
                                     "tanh|acosh|asinh|atanh|exp|ldexp|log|log10|"
                                     "sqrt|cbrt|tgamma|lgamma|ceil|floor|x|e)|^-|[0-9]?"
                                     "([0-9]*[.])?[0-9]+|[\\-\\+\\\\\(\\)\\/\\*\\^\\]",
                                      std::regex_constants::egrep);

        auto words_begin = std::sregex_iterator(str.begin(), str.end(), words_regex);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) 
        {
            res.push_back((*i).str());
        }
        return res;
    }

    //params: 
    //str - string to be converted
    //var_name - any occurances of this as a seperate token will be treated as a varable
    //convert string in infix notation to a string in Reverse Polish Notation
    //using dijkstra's shunting yard algorithm 
    std::vector<std::variant<double, std::string>> s_yard(const std::string& str, std::string var_name)
    {
        std::vector<std::variant<double, std::string>> output_queue;
        std::stack<std::string> op_stack;

        for (const auto& tok : tokenize(str))
        {
            if (tok == "pi")
            {
                output_queue.push_back(M_PI);
            }
            else if (tok == "e")
            {
                output_queue.push_back(exp(1));
            }
            else if (tok == var_name)
            {
                output_queue.push_back(tok);
            }
            else if (is_num(tok))
            {
                output_queue.push_back(strtod(tok.c_str(), NULL));
            }
            else if (is_func(tok))
            {
                op_stack.push(tok);
            }
            else if (is_binary_op(tok))
            {   
                while (!op_stack.empty() && \
                      (is_binary_op(op_stack.top()) && get_prec(op_stack.top()) > (get_prec(tok)) || \
                      (get_prec(op_stack.top()) == get_prec(tok) && is_left_assoc(tok))) && \
                      (op_stack.top() != "("))
                {
                    //pop operators from stack to queue
                    while (!op_stack.empty())
                    {
                        output_queue.push_back(op_stack.top());
                        op_stack.pop();
                    }
                }
                op_stack.push(tok);
            }
            else if (tok == "(")
            {
                op_stack.push(tok);
            }
            else if (tok == ")")
            {
                while (op_stack.top() != "(")
                {
                    output_queue.push_back(op_stack.top());
                    op_stack.pop();
                }
                if (op_stack.top() == "(")
                {
                    op_stack.pop();
                }
                if (is_func(op_stack.top()))
                {
                    output_queue.push_back(op_stack.top());
                    op_stack.pop();
                }
            }
        }
        //all tokens read
        while (!op_stack.empty())
        {
            //there are mismatched parentheses
            if (op_stack.top() == "(" || op_stack.top() == ")")
            {
                std::cout << "mismatched parentheses\n";
            }
            output_queue.push_back(op_stack.top());
            op_stack.pop();
        }
        return output_queue;
    }
    
    double compute_binary_ops(double d1, double d2, std::string_view op)
    {
        if (op == "*") return d1 * d2;
        else if (op == "+") return d1 + d2;
        else if (op == "-") return d1 - d2;
        else if (op == "/") return d1 / d2;
        else if (op == "^") return pow(d1, d2);
        else
        {
            std::cout << R"(invalid operator: ")" << op << R"("  passed to func "compute_binary_ops")" << '\n';
            exit(-1);
        }
    }

    double eval_rpn(const std::vector<std::variant<double, std::string>>& tokens, std::string var_name, double var_value)
    {   
        double d2 = 0.0;
        double res = 0.0;
        std::stack<std::variant<double, std::string>> stack;
        for (const auto& tok : tokens)
        {
            if (const double *number = std::get_if<double>(&tok))
            {
                stack.push(*number);
            }
            else if (std::get<std::string>(tok) == var_name)
            {
                stack.push(var_value);
            }
            //handle binary operaters
            else if(is_binary_op(std::get<std::string>(tok)))
            {
                d2 = std::get<double>(stack.top());
                stack.pop();
                if (!stack.empty())
                {       
                    const double d1 = std::get<double>(stack.top());
                    stack.pop();
                    res = compute_binary_ops(d1, d2, std::get<std::string>(tok));
                    stack.push(res);
                }
                else
                {
                    if (std::get<std::string>(tok) == "-") res = -(d2);
                    else res = d2;
                    stack.push(res);
                }
            }
            //handle funcs(unary ops)
            else if (is_func(std::get<std::string>(tok)))
            {
                if (!stack.empty())
                {
                    const double d1 = std::get<double>(stack.top());
                    stack.pop();
                    res = (*unary_func_tbl.at(std::get<std::string>(tok)))(d1);
                    stack.push(res);
                }
                else
                {
                    if (std::get<std::string>(tok) == "-") res = -(d2);
                    else res = d2;
                    stack.push(res);
                }
            }
        }
        return std::get<double>(stack.top());
    }
}

//from https://github.com/DOOMReboot/PixelPusher/blob/master/PixelPusher.cpp
namespace disp
{
    constexpr int32_t g_kRenderDeviceFlags = -1;
    constexpr  int32_t g_kErrorOccurred = -1;

    int32_t e(int32_t result, std::string errorMessage)
    {
        if (result) std::cout << errorMessage;
        return result;
    }

    SDL_Window* CreateCenteredWindow(uint32_t width, uint32_t height, std::string title)
    {
        // Get current device's Display Mode to calculate window position
        SDL_DisplayMode DM;
        SDL_GetCurrentDisplayMode(0, &DM);

        // Calculate where the upper-left corner of a centered window will be
        const int32_t x = DM.w / 2 - width / 2;
        const int32_t y = DM.h / 2 - height / 2;

        // Create the SDL window
        SDL_Window* pWindow = SDL_CreateWindow(win_title, x, y, screen_w, screen_h,
            SDL_WINDOW_ALLOW_HIGHDPI);

        if (e(!pWindow, "Failed to create Window\n"));

        return pWindow;
    }


    // Free resources 
    void Shutdown(SDL_Window** ppWindow, SDL_Renderer** ppRenderer, SDL_Texture** ppTexture)
    {
        // Free the Back Buffer
        if (ppTexture)
        {
            SDL_DestroyTexture(*ppTexture);
            *ppTexture = nullptr;
        }

        // Free the SDL renderer
        if (ppRenderer)
        {
            SDL_DestroyRenderer(*ppRenderer);
            *ppRenderer = nullptr;
        }

        // Free the SDL window
        if (ppWindow)
        {
            SDL_DestroyWindow(*ppWindow);
            *ppWindow = nullptr;
        }
        exit(0);
    }

}

//from https://stackoverflow.com/a/27030598
template<typename T>
std::vector<double> linspace(T start_in, T end_in, int num_in)
{
    std::vector<double> linspaced;
    linspaced.reserve(num_in);

    double start = static_cast<double>(start_in);
    double end = static_cast<double>(end_in);
    double num = static_cast<double>(num_in);

    if (num == 0) { return linspaced; }
    if (num == 1)
    {
        linspaced.push_back(start);
        return linspaced;
    }

    double delta = (end - start) / (num - 1);

    for (int i = 0; i < num - 1; ++i)
    {
        linspaced.push_back(start + delta * i);
    }
    linspaced.push_back(end); // I want to ensure that start and end
                              // are exactly the same as the input
    return linspaced;
}

template<typename T>
inline T sqr(T d)
{
    return d * d;
}

inline double dist_2d(const pt_2d& a, const pt_2d& b)
{
    return sqrt(sqr(b.x - a.x) + sqr(b.y - a.y));
}

void create_canvas(uint32_t *data)
{
    //create grid  
    for (int x = grid_spacing; x < screen_w; x += grid_spacing)
    {
        for (int y = 0; y < screen_h; y++)
        {
            data[x + (y * screen_w)] = green;
        }
    }
   
    for (int x = 0; x < screen_w; x++)
    {
        for (int y = grid_spacing; y < screen_h; y += grid_spacing)
        {
            data[x + (y * screen_w)] = green;
        }
    }

    //y axis
    for (int y = 0; y < screen_h; y++)
    {
        data[screen_w/2 + (y * screen_w)] = red;
    }
    //x axis
    for (int x = 0; x < screen_h; x++)
    {
        data[x + (screen_h/2 * screen_w)] = red;
    }   
}

void fill_gaps(uint32_t* data, const pt_2d& a, const pt_2d& b, int max)
{
    double dist = dist_2d(a, b);
    if (dist > 2 && dist < max)
    {
        std::vector<double> xvec = linspace(a.x, b.x, static_cast<int>(dist) + 1);
        std::vector<double> yvec = linspace(a.y, b.y, static_cast<int>(dist) + 1);
        for (int i = 0; i < xvec.size(); i++)
        {
            int ix = static_cast<int>(round(xvec[i]));
            int iy = static_cast<int>(round(yvec[i]));
            if (ix < screen_w - 1 && iy < screen_h - 1 && ix > 0 && iy > 0)
            {
                data[ix + (iy * screen_w)] = yellow;
            }
        }
    }
}

void plot(uint32_t* data, int range_lower, int range_upper, std::vector<std::variant<double, std::string>>& rpn, std::string var_name, float pt_step_count, int max)
{
    const int ratio = screen_w / range_upper;
    int lst_ix = 0;
    int lst_iy = 0;

    for (int x = (range_lower * pt_step_count); x < (range_upper * pt_step_count); x++)
    {
        double tx = x / pt_step_count;
        double ty = parser::eval_rpn(rpn, var_name, tx);

        tx *= (screen_w / static_cast<double>(range_upper));
        ty *= (screen_w / static_cast<double>(range_upper));

        int ix = -round(tx / 2.0);
        int iy = round(ty / 2.0);

        ix += screen_w / 2;
        iy += screen_h / 2;

        if (ix < screen_w - 1 && iy < screen_h - 1 && ix > 0 && iy > 0)
        {
            data[ix + (iy * screen_w)] = yellow;
        }
        
        fill_gaps(data, { lst_ix, lst_iy }, { ix, iy }, max);
        lst_ix = ix;
        lst_iy = iy;
    }
}

void render(SDL_Window* pWindow, SDL_Renderer* pRenderer, SDL_Texture* pTexture, uint32_t* data)
{
    int32_t pitch = 0;
    uint32_t* pPixelBuffer = nullptr;
    if (!SDL_LockTexture(pTexture, NULL, (void**)&pPixelBuffer, &pitch))
    {
        pitch /= sizeof(uint32_t);
        memcpy(pPixelBuffer, data, screen_h * static_cast<size_t>(pitch) * sizeof(uint32_t));
        SDL_UnlockTexture(pTexture);
        SDL_RenderCopy(pRenderer, pTexture, NULL, NULL);
        SDL_RenderPresent(pRenderer);
    }
}


int main()
{
    int range_upper = 5;
    int range_lower = -5;
    int max = 125;
    float pt_step_count = 1000;
    bool first_iter = true;
    SDL_Event e;
    uint32_t* data = new uint32_t[screen_w * screen_h];
    SDL_Window* pWindow = nullptr;
    SDL_Renderer* pRenderer = nullptr;
    SDL_Texture* pTexture = nullptr;
    std::string in_txt;
    std::vector<std::string> eqs_on_graph;

    SDL_Init(SDL_INIT_VIDEO);

    pWindow = disp::CreateCenteredWindow(screen_w, screen_h, win_title);
    pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED);
    pTexture = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);

    SDL_StartTextInput();
    create_canvas(data);
    render(pWindow, pRenderer, pTexture, data);

    std::cout << "zoom out with arrow down, zoom in with arrow up, clear with del\n";

    for(;;)
    {    
        in_txt.clear();
        while (SDL_WaitEvent(&e)) 
        {
            if (e.type == SDL_QUIT) 
            {
                disp::Shutdown(&pWindow, &pRenderer, &pTexture);
            }
            else if (e.type == SDL_TEXTINPUT) 
            {
                in_txt.append(e.text.text);
                std::cout << in_txt << '\r';              
            }
            else if (e.type == SDL_KEYDOWN) 
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) 
                {
                    disp::Shutdown(&pWindow, &pRenderer, &pTexture);
                }
                //clear
                else if (e.key.keysym.sym == SDLK_DELETE)
                {
                    eqs_on_graph.clear();
                    std::cout << "\033[2J" << "\033[1;1H";
                    memset(data, black, screen_w * screen_h * sizeof(uint32_t));
                    create_canvas(data);
                    render(pWindow, pRenderer, pTexture, data);
                    continue;
                }
                //zoom in
                else if (e.key.keysym.sym == SDLK_UP)
                {
                    if ((range_upper - 1) > 0 && (range_lower + 1) < 0)
                    {
                        range_lower++;
                        range_upper--;
                    }
                    
                    pt_step_count += 2;
                    max--;
                    if (pt_step_count >= 1000)
                    {
                        pt_step_count = 1000;
                    }
                    if (max <= 200)
                    {
                        max = 200;
                    }

                    if (!eqs_on_graph.empty())
                    {
                        memset(data, black, screen_w * screen_h * sizeof(uint32_t));
                        create_canvas(data);
                        for (const std::string& last_txt : eqs_on_graph)
                        {
                            std::vector<std::variant<double, std::string>> rpn = parser::s_yard(last_txt, var_name);
                            plot(data, range_lower, range_upper, rpn, var_name, pt_step_count, max);                   
                        }
                        render(pWindow, pRenderer, pTexture, data);
                    }
                    std::cout << std::string(20, ' ') << '\r';
                    std::cout << "range: " << range_lower << " to: " << range_upper << '\r';
                    continue;
                }
                //zoom out
                else if (e.key.keysym.sym == SDLK_DOWN)
                {
                    range_lower--;
                    range_upper++;
                    max++;
                    pt_step_count -= 2;
                    if (pt_step_count <= 200)
                    {
                        pt_step_count = 200;
                    }
                    if (max >= 500)
                    {
                        max = 500;
                    }

                    if (!eqs_on_graph.empty())
                    {
                        memset(data, black, screen_w * screen_h * sizeof(uint32_t));
                        create_canvas(data);
                        for (const std::string& last_txt : eqs_on_graph)
                        {
                            std::vector<std::variant<double, std::string>>rpn = parser::s_yard(last_txt, var_name);
                            plot(data, range_lower, range_upper, rpn, var_name, pt_step_count, max);    
                        }
                        render(pWindow, pRenderer, pTexture, data);
                    }
                    std::cout << std::string(20, ' ') << '\r';
                    std::cout << "range: " << range_lower << " to: " << range_upper << '\r';
                    continue;
                }
                else if (e.key.keysym.sym == SDLK_BACKSPACE) 
                {
                    if (in_txt.size() > 0) 
                    {                          
                        in_txt.erase(in_txt.size() - 1);                           
                        std::cout << in_txt << std::string(in_txt.size() + 1, ' ') << '\r';
                    }
                }
            }
            else if (e.type == SDL_KEYUP) 
            {
                if (e.key.keysym.sym == SDLK_RETURN)
                {
                    std::cout << '\n';
                    break;
                }
            }
        }

        eqs_on_graph.push_back(in_txt);

        //wont evaluate as true if the input is one letter and not var name
        if (!first_iter && !in_txt.empty() && !(in_txt != var_name && in_txt.size() == 1))
        {
            std::vector<std::variant<double, std::string>> rpn = parser::s_yard(in_txt, var_name);
            plot(data, range_lower, range_upper, rpn, var_name, pt_step_count, max);
            render(pWindow, pRenderer, pTexture, data);
        }
        else if (!in_txt.empty() && !(in_txt != var_name && in_txt.size() == 1)) 
        {
            std::vector<std::variant<double, std::string>> rpn = parser::s_yard(in_txt, var_name);
            plot(data, range_lower, range_upper, rpn, var_name, pt_step_count, max);
            render(pWindow, pRenderer, pTexture, data);
            first_iter = false;
        }       
    }   
    return 0;
}