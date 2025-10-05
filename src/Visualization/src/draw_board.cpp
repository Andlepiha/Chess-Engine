#include "../headers/draw_board.hpp"
#include <cassert>
#include <format>
#include <math.h>

Board::Board(sf::Vector2u window_size, std::string data_dir, bool flipped)
    : flipped(flipped)
{
    this->window_size = window_size;
    label_font.loadFromFile(std::format("{}/inconsolata.ttf", data_dir));

    squares.setUsage(sf::VertexBuffer::Dynamic);
    squares.setPrimitiveType(sf::Quads);
    squares.create(64 * 4);

    this->resize(window_size);

    w_pawn_texture = this->parse_svg_file(std::format("{}/w_pn.svg", data_dir).c_str());
    b_pawn_texture = this->parse_svg_file(std::format("{}/b_pn.svg", data_dir).c_str());

    w_knight_texture = this->parse_svg_file(std::format("{}/w_kn.svg", data_dir).c_str());
    b_knight_texture = this->parse_svg_file(std::format("{}/b_kn.svg", data_dir).c_str());

    w_bishop_texture = this->parse_svg_file(std::format("{}/w_bs.svg", data_dir).c_str());
    b_bishop_texture = this->parse_svg_file(std::format("{}/b_bs.svg", data_dir).c_str());

    w_rook_texture = this->parse_svg_file(std::format("{}/w_rk.svg", data_dir).c_str());
    b_rook_texture = this->parse_svg_file(std::format("{}/b_rk.svg", data_dir).c_str());

    w_queen_texture = this->parse_svg_file(std::format("{}/w_qn.svg", data_dir).c_str());
    b_queen_texture = this->parse_svg_file(std::format("{}/b_qn.svg", data_dir).c_str());

    w_king_texture = this->parse_svg_file(std::format("{}/w_kg.svg", data_dir).c_str());
    b_king_texture = this->parse_svg_file(std::format("{}/b_kg.svg", data_dir).c_str());
}

void Board::resize(sf::Vector2u new_window_size)
{
    float new_size = std::min(new_window_size.x, new_window_size.y);

    this->board_size = static_cast<int>(static_cast<float>(new_size) * board_scale * inner_board_scale);
    this->board_offset = {(new_window_size.x - this->board_size) / 2.0f,
                          (new_window_size.y - this->board_size) / 2.0f};
    this->square_size = board_size / 8;

    place_border();
    place_labels();
    place_squares();
}

void Board::draw_board(sf::RenderWindow *window, movgen::BoardPosition *pos)
{
    window->draw(squares);
    window->draw(char_label);
    window->draw(number_label);
    window->draw(inner);
    window->draw(outer);

    this->draw_pieces(window, pos);
}

void Board::draw_piece_moves(sf::RenderWindow *window, std::vector<movgen::Move> &moves)
{
    sf::CircleShape move_dot;
    move_dot.setFillColor({155, 155, 155, 0});
    move_dot.setOutlineColor({200, 200, 200, 220});
    move_dot.setOutlineThickness(square_size * 0.15f);
    move_dot.setRadius(square_size / 2.0f * 0.5f);

    for (auto &move : moves)
    {
        sf::Vector2f dot_pos = board_to_screen({
            square_size * (move.to % 8),
            square_size * (move.to / 8),
        });
        move_dot.setPosition({dot_pos.x + (square_size / 2.0f - move_dot.getRadius()),
                              dot_pos.y + (square_size / 2.0f - move_dot.getRadius())});

        window->draw(move_dot);
    }
}

void Board::draw_check(sf::RenderWindow* window, uint16_t check_square)
{
    const sf::Color inner_color = sf::Color(235, 25, 25, 200);
    const sf::Color outer_color = sf::Color(235, 25, 25, 0);

    const float radius = square_size / 2.0f;
    const sf::Vector2f center_pos = board_to_screen({
            square_size * (check_square % 8) - radius,
            square_size * (check_square / 8) - radius,
        });

    sf::VertexArray circle(sf::TriangleStrip, 50);

    for (size_t i = 0; i < circle.getVertexCount(); i += 2)
    {
        float angle = 2 * 3.14 * (i / float(circle.getVertexCount() - 2));

        circle[i].position = center_pos;
        circle[i].color = inner_color;

        circle[i + 1].position = {
            center_pos.x + cos(angle) * radius,
            center_pos.y + sin(angle) * radius };
        circle[i + 1].color = outer_color;
    }

    window->draw(circle);
}

void Board::flip_board()
{
    this->flipped = !this->flipped;
    this->place_labels();
    this->place_squares();
}

bool Board::is_flipped()
{
    return flipped;
}

void Board::select_square(sf::Vector2i mouse_pos)
{
    assert(within_bounds(mouse_pos.x, mouse_pos.y));

    sf::Vector2f board_pos = screen_to_board(static_cast<sf::Vector2f>(mouse_pos));

    this->select_square(
        static_cast<int>(board_pos.x / this->square_size),
        static_cast<int>(board_pos.y / this->square_size));
}

void Board::select_square(int x_pos, int y_pos)
{
    assert(x_pos < 8 && x_pos >= 0 && y_pos < 8 && y_pos >= 0);

    int new_select = y_pos * 8 + x_pos;
    const sf::Color sel_col = choose_color(new_select, selected_dark, selected_light);

    if (new_select == this->selected_square)
    {
        update_squares(this->selected_square, NO_SQUARE);
        this->selected_square = -1;
    }
    else
    {
        update_squares(this->selected_square, new_select, sel_col);
        this->selected_square = new_select;
    }
}

void Board::deselect_square()
{
    update_squares(this->selected_square, NO_SQUARE);
    this->selected_square = -1;
}

int Board::get_selected_square()
{
    return this->selected_square;
}

void Board::highlight_prev_move(uint16_t old_index, uint16_t new_index)
{
    const sf::Color col = choose_color(new_index, prev_move_dark, prev_move_light);
    update_squares(old_index, new_index, col);
}

sf::Vector2f Board::screen_to_board(sf::Vector2f screen_pos)
{
    // TODO: finish this
    sf::Vector2f ret_vec;

    float scale = inner_board_scale * board_scale;

    if (flipped)
    {
        ret_vec.x = (screen_pos.x - board_offset.x);
        ret_vec.y = (screen_pos.y - board_offset.y);
    }
    else
    {
        ret_vec.x = board_size - (screen_pos.x - board_offset.x);
        ret_vec.y = board_size - (screen_pos.y - board_offset.y);
    }

    return ret_vec;
}

sf::Vector2f Board::board_to_screen(sf::Vector2f board_pos)
{
    // TODO: finish this
    sf::Vector2f ret_vec;

    if (flipped)
    {
        ret_vec.x = (board_pos.x + board_offset.x);
        ret_vec.y = (board_pos.y + board_offset.y);
    }
    else
    {
        ret_vec.x = board_size - (board_pos.x - board_offset.x) - square_size;
        ret_vec.y = board_size - (board_pos.y - board_offset.y) - square_size;
    }

    return ret_vec;
}

bool Board::within_bounds(float mouse_x, float mouse_y)
{
    return (mouse_x >= board_offset.x && mouse_x <= (board_offset.x + board_size) &&
            mouse_y >= board_offset.y && mouse_y <= (board_offset.y + board_size));
}

float Board::get_square_size()
{
    return this->square_size;
}

float Board::get_size()
{
    return board_size;
}

sf::Color Board::choose_color(uint16_t square_index, sf::Color dark, sf::Color bright)
{
    return (square_index % 2) ^ (square_index % 16 >= 8) ? dark : bright;
}

void Board::place_border()
{
    sf::Vector2f board_center = {window_size.x / 2.0f, window_size.y / 2.0f};
    sf::Vector2f center_origin = {board_size / 2.0f, board_size / 2.0f};

    outer.setSize({board_size, board_size});
    inner.setSize({board_size, board_size});

    outer.setOrigin(center_origin);
    inner.setOrigin(center_origin);

    outer.setScale({1 / inner_board_scale, 1 / inner_board_scale});

    outer.setPosition(board_center);
    inner.setPosition(board_center);

    outer.setOutlineThickness(window_size.x / 250.0f);
    inner.setOutlineThickness(window_size.x / 250.0f);

    outer.setOutlineColor(sf::Color::Black);
    inner.setOutlineColor(sf::Color::Black);

    outer.setFillColor(sf::Color::Transparent);
    inner.setFillColor(sf::Color::Transparent);
}

void Board::place_labels()
{
    // Set label size to half of distance between borders
    float label_size = (board_size * (1 - inner_board_scale)) * 0.5f;
    float char_width = (sf::Text("a", label_font)).getLocalBounds().width;

    char_label.setFont(label_font);
    char_label.setFillColor(sf::Color::Black);
    char_label.setLetterSpacing(13.0f);
    char_label.setCharacterSize(label_size);
    char_label.setPosition({(window_size.x - board_size - char_width + square_size) / 2.0f - char_label.getLetterSpacing() / 2.0f,
                            (window_size.y + board_size) / 2.0f - char_label.getLetterSpacing() / 2.0f});

    number_label.setFont(label_font);
    number_label.setFillColor(sf::Color::Black);
    number_label.setLetterSpacing(13.0f);
    number_label.setCharacterSize(label_size);
    number_label.setPosition({(window_size.x - board_size - label_size * 2.5f) / 2.0f,
                              (window_size.y + board_size + char_width - square_size) / 2.0f + number_label.getLetterSpacing() / 2.0f});
    number_label.setRotation(270);

    if (!this->flipped)
    {
        char_label.setString("abcdefgh");
        number_label.setString("12345678");
    }
    else
    {
        char_label.setString("hgfedcba");
        number_label.setString("87654321");
    }
}

void Board::place_squares()
{
    sf::Color cur_col;

    if (this->square_vertex != nullptr)
        delete[] this->square_vertex;
    this->square_vertex = new sf::Vertex[64 * 4];

    for (int i = 0; i < 64; i++)
    {
        this->square_vertex[i * 4 + 0].position = {
            board_offset.x + (i % 8 + 0) * (square_size),
            board_offset.y + (i / 8 + 0) * (square_size)};
        this->square_vertex[i * 4 + 1].position = {
            board_offset.x + (i % 8 + 1) * (square_size),
            board_offset.y + (i / 8 + 0) * (square_size)};
        this->square_vertex[i * 4 + 2].position = {
            board_offset.x + (i % 8 + 1) * (square_size),
            board_offset.y + (i / 8 + 1) * (square_size)};
        this->square_vertex[i * 4 + 3].position = {
            board_offset.x + (i % 8 + 0) * (square_size),
            board_offset.y + (i / 8 + 1) * (square_size)};

        cur_col = choose_color(i, board_green, board_pale);

        this->square_vertex[i * 4 + 0].color = cur_col;
        this->square_vertex[i * 4 + 1].color = cur_col;
        this->square_vertex[i * 4 + 2].color = cur_col;
        this->square_vertex[i * 4 + 3].color = cur_col;
    }
    this->squares.update(this->square_vertex);
}

void Board::update_squares(uint16_t reset_pos, uint16_t set_pos, sf::Color set_col)
{
	if (!flipped)
	{
		reset_pos = reset_pos == NO_SQUARE ? reset_pos : 63 - reset_pos;
		set_pos = set_pos == NO_SQUARE ? set_pos : 63 - set_pos;
	}

    const sf::Color reset_col = choose_color(reset_pos, board_green, board_pale);

    if (reset_pos != NO_SQUARE)
    {
        this->square_vertex[reset_pos * 4 + 0].color = reset_col;
        this->square_vertex[reset_pos * 4 + 1].color = reset_col;
        this->square_vertex[reset_pos * 4 + 2].color = reset_col;
        this->square_vertex[reset_pos * 4 + 3].color = reset_col;
    }

    if (set_pos != NO_SQUARE)
    {
        this->square_vertex[set_pos * 4 + 0].color = set_col;
        this->square_vertex[set_pos * 4 + 1].color = set_col;
        this->square_vertex[set_pos * 4 + 2].color = set_col;
        this->square_vertex[set_pos * 4 + 3].color = set_col;
    }

    this->squares.update(this->square_vertex);
}

void Board::draw_pieces(sf::RenderWindow *window, movgen::BoardPosition *pos)
{
    movgen::Piece cur_piece;
    sf::RectangleShape piece;
    piece.setSize({square_size, square_size});

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            movgen::Piece cur_piece = movgen::get_piece(*pos, i * 8 + j);

            if (static_cast<int>(cur_piece) != 0)
            {
                sf::Texture *cur_texture = get_piece_texture(cur_piece);

                piece.setTexture(cur_texture);
                piece.setPosition(this->board_to_screen({(j)*square_size, (i)*square_size}));
                // knight.setFillColor(sf::Color(0, 0, 0, 125));
                window->draw(piece);
            }
        }
    }
}

sf::Texture *Board::get_piece_texture(movgen::Piece piece)
{
    switch (piece)
    {
    case movgen::Piece::B_KING:
        return &b_king_texture;
    case movgen::Piece::W_KING:
        return &w_king_texture;
    case movgen::Piece::B_QUEEN:
        return &b_queen_texture;
    case movgen::Piece::W_QUEEN:
        return &w_queen_texture;
    case movgen::Piece::B_ROOK:
        return &b_rook_texture;
    case movgen::Piece::W_ROOK:
        return &w_rook_texture;
    case movgen::Piece::B_BISHOP:
        return &b_bishop_texture;
    case movgen::Piece::W_BISHOP:
        return &w_bishop_texture;
    case movgen::Piece::B_KNIGHT:
        return &b_knight_texture;
    case movgen::Piece::W_KNIGHT:
        return &w_knight_texture;
    case movgen::Piece::B_PAWN:
        return &b_pawn_texture;
    case movgen::Piece::W_PAWN:
        return &w_pawn_texture;
    default:
        return nullptr;
    }

    return nullptr;
}

sf::Texture Board::parse_svg_file(const char *filename)
{
    NSVGimage *image_path;
    image_path = nsvgParseFromFile(filename, "px", 96.0f);

    if (!std::filesystem::exists(filename))
    {
        std::string err_string = std::format("Cound not find image file: {}", filename);
        throw std::runtime_error(err_string.c_str());
    }

    static NSVGrasterizer *rast = nullptr;
    if (rast == nullptr)
    {
        rast = nsvgCreateRasterizer();
    }

    int w = static_cast<int>(this->square_size), h = static_cast<int>(this->square_size);
    float scale = this->square_size / image_path->width;
    // int w = 35; int h = 35;
    unsigned char *image = static_cast<unsigned char *>(malloc(w * h * 4));
    nsvgRasterize(rast, image_path, 0, 0, scale, image, w, h, w * 4);

    sf::Image sfml_image;
    sfml_image.create(w, h, image);

    nsvgDelete(image_path);

    sf::Texture image_texture;
    image_texture.create(w, h);
    image_texture.update(sfml_image);
    image_texture.setSmooth(false);

    return image_texture;
}
