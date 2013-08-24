#include <string>
#include <sstream>
#include <vector>
#include <jansson.h>
#include "themoviedbscraper.h"

using namespace std;

cMovieDBScraper::cMovieDBScraper(string baseDir, cTVScraperDB *db, string language) {
    apiKey = "abb01b5a277b9c2c60ec0302d83c5ee9";
    //TODO: use language from VDR settings
    this->language = language;
    baseURL = "api.themoviedb.org/3";
    this->baseDir = baseDir;
    this->db = db;
    posterSize = "w500";
    backdropSize = "w1280";
    actorthumbSize = "h632";
}

cMovieDBScraper::~cMovieDBScraper() {
}

void cMovieDBScraper::Scrap(const cEvent *event, int recordingID) {
    string movieName = event->Title();
    int eventID = (int)event->EventID();
    if (config.enableDebug)
        esyslog("tvscraper: scrapping movie \"%s\"", movieName.c_str());
    int movieID = SearchMovie(movieName);
    if (movieID < 1) {
        if (config.enableDebug)
            esyslog("tvscraper: nothing found for \"%s\"", movieName.c_str());
        return;
    }
    if (!recordingID) {
        time_t validTill = event->EndTime();
        db->InsertEventMovie(eventID, validTill, movieID);
    } else {
        db->InsertRecording(recordingID, 0, movieID);
    }
    if (db->MovieExists(movieID)) {
        return;
    }
    cMovieDbMovie *movie = ReadMovie(movieID);
    if (!movie)
        return;
    movie->StoreDB(db);
    cMovieDbActors *actors = ReadActors(movieID);
    if (!actors) {
        delete movie;
        return;
    }
    actors->StoreDB(db, movieID);
    StoreMedia(movie, actors);
    delete movie;
    delete actors;
    if (config.enableDebug)
        esyslog("tvscraper: \"%s\" successfully scrapped, id %d", movieName.c_str(), movieID);
}

bool cMovieDBScraper::Connect(void) {
    stringstream url;
    url << baseURL << "/configuration?api_key=" << apiKey;
    string configJSON;
    if (CurlGetUrl(url.str().c_str(), &configJSON)) {
        return parseJSON(configJSON);
    }
    return false;
}

bool cMovieDBScraper::parseJSON(string jsonString) {
    json_t *root;
    json_error_t error;

    root = json_loads(jsonString.c_str(), 0, &error);
    if (!root) {
        return false;
    }
    if(!json_is_object(root)) {
        return false;
    }
    json_t *images;
    images = json_object_get(root, "images");
    if(!json_is_object(images)) {
        return false;
    }
    
    json_t *imgUrl;
    imgUrl = json_object_get(images, "base_url");
    if(!json_is_string(imgUrl)) {
        return false;
    }
    imageUrl = json_string_value(imgUrl);
    return true;
}

int cMovieDBScraper::SearchMovie(string movieName) {
    stringstream movieEscaped;
    movieEscaped << "\"" << movieName << "\"";
    stringstream url;
    url << baseURL << "/search/movie?api_key=" << apiKey << "&query=" << CurlEscape(movieEscaped.str().c_str()) << "&language=" << language.c_str();
    string movieJSON;
    int movieID = -1;
    if (CurlGetUrl(url.str().c_str(), &movieJSON)) {
        cMovieDbMovie *movie = new cMovieDbMovie(movieJSON);
        movieID = movie->ParseJSONForMovieId(movieName);
        delete movie;
    }
    return movieID;
}

cMovieDbMovie *cMovieDBScraper::ReadMovie(int movieID) {
    stringstream url;
    url << baseURL << "/movie/" << movieID << "?api_key=" << apiKey << "&language=" << language.c_str();
    string movieJSON;
    cMovieDbMovie *movie = NULL;
    if (CurlGetUrl(url.str().c_str(), &movieJSON)) {
        movie = new cMovieDbMovie(movieJSON);
        movie->SetID(movieID);
        movie->ParseJSON();
    }
    return movie;
}

cMovieDbActors *cMovieDBScraper::ReadActors(int movieID) {
    stringstream url;
    url << baseURL << "/movie/" << movieID << "/casts?api_key=" << apiKey;
    string actorsJSON;
    cMovieDbActors *actors = NULL;
    if (CurlGetUrl(url.str().c_str(), &actorsJSON)) {
        actors = new cMovieDbActors(actorsJSON);
        actors->ParseJSON();
    }
    return actors;
}

void cMovieDBScraper::StoreMedia(cMovieDbMovie *movie, cMovieDbActors *actors) {
    stringstream posterUrl;
    posterUrl << imageUrl << posterSize;
    stringstream backdropUrl;
    backdropUrl << imageUrl << backdropSize;
    stringstream destDir;
    destDir << baseDir << "/";
    movie->StoreMedia(posterUrl.str(), backdropUrl.str(), destDir.str());
    stringstream actorsUrl;
    actorsUrl << imageUrl << actorthumbSize;
    stringstream actorsDestDir;
    actorsDestDir << baseDir << "/actors";
    CreateDirectory(actorsDestDir.str());
    actors->Store(actorsUrl.str(), actorsDestDir.str());
}