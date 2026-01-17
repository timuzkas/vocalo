#include "httplib.h"
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <cstdlib>
#include <map>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

// ==========================================
// Utility Functions
// ==========================================
fs::path get_data_directory() {
    fs::path app_dir;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        app_dir = fs::path(path) / "vocalo";
    } else {
        app_dir = fs::current_path();
    }
#elif __APPLE__
    const char* home = getenv("HOME");
    if (home) app_dir = fs::path(home) / "Library" / "Application Support" / "vocalo";
#else
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg) app_dir = fs::path(xdg) / "vocalo";
    else {
        const char* home = getenv("HOME");
        if (home) app_dir = fs::path(home) / ".local" / "share" / "vocalo";
        else app_dir = fs::current_path();
    }
#endif
    if (!fs::exists(app_dir)) fs::create_directories(app_dir);
    return app_dir;
}

void open_browser(const string& url) {
#ifdef _WIN32
    string cmd = "start " + url;
#elif __APPLE__
    string cmd = "open " + url;
#else
    string cmd = "xdg-open " + url;
#endif
    system(cmd.c_str());
}

// Simple JSON helpers
string escape_json(const string& s) {
    string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

string read_file_content(const string& path) {
    ifstream file(path);
    if (!file.is_open()) return "<h1>Error: " + path + " not found</h1>";
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

// ==========================================
// Data Structures
// ==========================================
struct Word {
    string word;
    string translation;
    string definition;
    string example;
    string hint;
};

struct Deck {
    string name;
    string description;
    vector<Word> words;
};

struct Session {
    long long timestamp;
    string deck;
    int correct;
    int total;
    int score;
    string mode;
};

map<string, Deck> decks;
vector<Session> sessions;

// ==========================================
// Persistence
// ==========================================
fs::path get_decks_file() { return get_data_directory() / "decks.json"; }
fs::path get_sessions_file() { return get_data_directory() / "sessions.txt"; }

void load_decks() {
    decks.clear();
    ifstream file(get_decks_file());
    if (!file.is_open()) {
        // Create sample deck
        decks["sample"] = {
            "Spanish Basics",
            "Common Spanish words",
            {
                {"hola", "hello", "a greeting", "¡Hola! ¿Cómo estás?", ""},
                {"gracias", "thank you", "expression of gratitude", "Muchas gracias", ""},
                {"agua", "water", "H2O, liquid for drinking", "Quiero agua", ""},
                {"casa", "house", "a building for living", "Mi casa es tu casa", ""},
                {"libro", "book", "written or printed work", "Leo un libro", ""},
                {"perro", "dog", "domesticated canine", "El perro es grande", ""},
                {"gato", "cat", "domesticated feline", "El gato duerme", ""},
                {"tiempo", "time/weather", "duration or atmospheric conditions", "¿Qué tiempo hace?", ""},
                {"amigo", "friend", "a person you know and like", "Él es mi amigo", ""},
                {"comida", "food", "something to eat", "La comida está lista", ""}
            }
        };
        return;
    }

    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    // Simple JSON parse (production would use proper library)
    size_t pos = 0;
    while ((pos = content.find("\"deck_", pos)) != string::npos || 
           (pos == 0 && (pos = content.find("\"sample\"", pos)) != string::npos)) {
        size_t idStart = pos + 1;
        size_t idEnd = content.find("\"", idStart);
        string id = content.substr(idStart, idEnd - idStart);
        
        size_t namePos = content.find("\"name\"", idEnd);
        if (namePos == string::npos) break;
        size_t nameStart = content.find("\"", namePos + 6) + 1;
        size_t nameEnd = content.find("\"", nameStart);
        string name = content.substr(nameStart, nameEnd - nameStart);

        Deck deck;
        deck.name = name;

        size_t descPos = content.find("\"description\"", nameEnd);
        if (descPos != string::npos && descPos < content.find("\"words\"", nameEnd)) {
            size_t descStart = content.find("\"", descPos + 13) + 1;
            size_t descEnd = content.find("\"", descStart);
            deck.description = content.substr(descStart, descEnd - descStart);
        }

        size_t wordsStart = content.find("[", content.find("\"words\"", nameEnd));
        size_t wordsEnd = content.find("]", wordsStart);
        string wordsStr = content.substr(wordsStart, wordsEnd - wordsStart + 1);

        size_t wpos = 0;
        while ((wpos = wordsStr.find("{", wpos)) != string::npos) {
            Word w;
            size_t wend = wordsStr.find("}", wpos);
            string wordObj = wordsStr.substr(wpos, wend - wpos);

            auto getField = [&](const string& field) -> string {
                size_t fp = wordObj.find("\"" + field + "\"");
                if (fp == string::npos) return "";
                size_t vs = wordObj.find("\"", fp + field.length() + 2) + 1;
                size_t ve = wordObj.find("\"", vs);
                return wordObj.substr(vs, ve - vs);
            };

            w.word = getField("word");
            w.translation = getField("translation");
            w.definition = getField("definition");
            w.example = getField("example");
            w.hint = getField("hint");

            if (!w.word.empty()) deck.words.push_back(w);
            wpos = wend + 1;
        }

        decks[id] = deck;
        pos = wordsEnd;
    }
}

void save_decks() {
    ofstream file(get_decks_file());
    file << "{\n";
    bool first = true;
    for (const auto& [id, deck] : decks) {
        if (!first) file << ",\n";
        first = false;
        file << "  \"" << id << "\": {\n";
        file << "    \"name\": \"" << escape_json(deck.name) << "\",\n";
        file << "    \"description\": \"" << escape_json(deck.description) << "\",\n";
        file << "    \"words\": [\n";
        for (size_t i = 0; i < deck.words.size(); i++) {
            const auto& w = deck.words[i];
            file << "      {\"word\": \"" << escape_json(w.word) 
                 << "\", \"translation\": \"" << escape_json(w.translation)
                 << "\", \"definition\": \"" << escape_json(w.definition)
                 << "\", \"example\": \"" << escape_json(w.example)
                 << "\", \"hint\": \"" << escape_json(w.hint) << "\"}";
            if (i < deck.words.size() - 1) file << ",";
            file << "\n";
        }
        file << "    ]\n  }";
    }
    file << "\n}\n";
}

void load_sessions() {
    sessions.clear();
    ifstream file(get_sessions_file());
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        Session s;
        if (iss >> s.timestamp >> s.deck >> s.correct >> s.total >> s.score >> s.mode) {
            sessions.push_back(s);
        }
    }
}

void save_session(const Session& s) {
    sessions.push_back(s);
    ofstream file(get_sessions_file(), ios::app);
    file << s.timestamp << " " << s.deck << " " << s.correct << " " 
         << s.total << " " << s.score << " " << s.mode << "\n";
}

// ==========================================
// Main
// ==========================================
int main(int argc, char* argv[]) {
    int port = 8080;
    bool should_open_browser = true;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        } else if (arg == "--no-browser") {
            should_open_browser = false;
        }
    }

    cout << "Vocalo - Language Learning App\n";
    cout << "Data: " << get_data_directory() << "\n";

    load_decks();
    load_sessions();

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(read_file_content("index.html"), "text/html");
    });

    svr.Get("/api/decks", [](const httplib::Request&, httplib::Response& res) {
        string json = "{";
        bool first = true;
        for (const auto& [id, deck] : decks) {
            if (!first) json += ",";
            first = false;
            json += "\"" + id + "\":{";
            json += "\"name\":\"" + escape_json(deck.name) + "\",";
            json += "\"description\":\"" + escape_json(deck.description) + "\",";
            json += "\"words\":[";
            for (size_t i = 0; i < deck.words.size(); i++) {
                const auto& w = deck.words[i];
                json += "{\"word\":\"" + escape_json(w.word) + "\",";
                json += "\"translation\":\"" + escape_json(w.translation) + "\",";
                json += "\"definition\":\"" + escape_json(w.definition) + "\",";
                json += "\"example\":\"" + escape_json(w.example) + "\",";
                json += "\"hint\":\"" + escape_json(w.hint) + "\"}";
                if (i < deck.words.size() - 1) json += ",";
            }
            json += "]}";
        }
        json += "}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/save-deck", [](const httplib::Request& req, httplib::Response& res) {
        // Parse JSON body
        string b = req.body;
        auto getStr = [&](const string& key) -> string {
            size_t pos = b.find("\"" + key + "\"");
            if (pos == string::npos) return "";
            size_t start = b.find("\"", pos + key.length() + 2) + 1;
            size_t end = start;
            while (end < b.size() && !(b[end] == '"' && b[end-1] != '\\')) end++;
            return b.substr(start, end - start);
        };

        string id = getStr("id");
        Deck deck;
        deck.name = getStr("name");
        deck.description = getStr("description");

        // Parse words array
        size_t wordsStart = b.find("\"words\"");
        if (wordsStart != string::npos) {
            size_t arrStart = b.find("[", wordsStart);
            size_t arrEnd = b.rfind("]");
            string arr = b.substr(arrStart, arrEnd - arrStart + 1);

            size_t pos = 0;
            while ((pos = arr.find("{", pos)) != string::npos) {
                size_t end = arr.find("}", pos);
                string obj = arr.substr(pos, end - pos + 1);

                auto getField = [&](const string& f) -> string {
                    size_t fp = obj.find("\"" + f + "\"");
                    if (fp == string::npos) return "";
                    size_t vs = obj.find("\"", fp + f.length() + 2) + 1;
                    size_t ve = vs;
                    while (ve < obj.size() && !(obj[ve] == '"' && obj[ve-1] != '\\')) ve++;
                    return obj.substr(vs, ve - vs);
                };

                Word w;
                w.word = getField("word");
                w.translation = getField("translation");
                w.definition = getField("definition");
                w.example = getField("example");
                w.hint = getField("hint");
                if (!w.word.empty()) deck.words.push_back(w);

                pos = end + 1;
            }
        }

        if (!id.empty() && !deck.name.empty()) {
            decks[id] = deck;
            save_decks();
        }

        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Delete("/api/delete-deck", [](const httplib::Request& req, httplib::Response& res) {
        string b = req.body;
        size_t pos = b.find("\"id\"");
        if (pos != string::npos) {
            size_t start = b.find("\"", pos + 4) + 1;
            size_t end = b.find("\"", start);
            string id = b.substr(start, end - start);
            decks.erase(id);
            save_decks();
        }
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/save-session", [](const httplib::Request& req, httplib::Response& res) {
        string b = req.body;
        auto getInt = [&](const string& key) -> int {
            size_t pos = b.find("\"" + key + "\"");
            if (pos == string::npos) return 0;
            size_t start = b.find(":", pos) + 1;
            while (start < b.size() && !isdigit(b[start]) && b[start] != '-') start++;
            return stoi(b.substr(start));
        };
        auto getStr = [&](const string& key) -> string {
            size_t pos = b.find("\"" + key + "\"");
            if (pos == string::npos) return "";
            size_t start = b.find("\"", pos + key.length() + 2) + 1;
            size_t end = b.find("\"", start);
            return b.substr(start, end - start);
        };

        Session s;
        s.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        s.deck = getStr("deck");
        s.correct = getInt("correct");
        s.total = getInt("total");
        s.score = getInt("score");
        s.mode = getStr("mode");
        save_session(s);

        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Get("/api/sessions", [](const httplib::Request&, httplib::Response& res) {
        string json = "[";
        for (size_t i = 0; i < sessions.size(); i++) {
            const auto& s = sessions[i];
            json += "{";
            json += "\"timestamp\":" + to_string(s.timestamp) + ",";
            json += "\"deck\":\"" + escape_json(s.deck) + "\",";
            json += "\"correct\":" + to_string(s.correct) + ",";
            json += "\"total\":" + to_string(s.total) + ",";
            json += "\"score\":" + to_string(s.score) + ",";
            json += "\"mode\":\"" + escape_json(s.mode) + "\"";
            json += "}";
            if (i < sessions.size() - 1) json += ",";
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    cout << "Starting at http://localhost:" << port << "\n";
    if (should_open_browser) {
        thread([port]() {
            this_thread::sleep_for(chrono::seconds(1));
            open_browser("http://localhost:" + to_string(port));
        }).detach();
    }

    svr.listen("localhost", port);
}
