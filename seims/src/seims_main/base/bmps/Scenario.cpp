#include "Scenario.h"

namespace MainBMP {
Scenario::Scenario(mongoc_client_t *conn, string dbName, int scenarioID) :
    m_conn(conn), m_bmpDBName(dbName) {
    if (scenarioID >= 0) {
        this->m_id = scenarioID;
    } else {
        throw ModelException("LoadBMPsScenario", "InitiateScenario",
                             "The scenario ID must be greater than or equal to 0.\n");
    }
    loadScenario();
}

Scenario::~Scenario(void) {
    map<int, BMPFactory *>::iterator it;
    for (it = this->m_bmpFactories.begin(); it != this->m_bmpFactories.end();) {
        if (it->second != NULL) delete (it->second);
        it = m_bmpFactories.erase(it);
    }
    m_bmpFactories.clear();
}

void Scenario::loadScenario() {
    MongoDatabase(m_conn, m_bmpDBName).getCollectionNames(m_bmpCollections);
    loadScenarioName();
    loadBMPs();
    loadBMPDetail();
}

void Scenario::loadScenarioName() {
    vector<string>::iterator it = find(m_bmpCollections.begin(), m_bmpCollections.end(), string(TAB_BMP_SCENARIO));
    if (it == m_bmpCollections.end()) {
        throw ModelException("BMP Scenario", "loadScenarioName", "The BMP database '" + m_bmpDBName +
            "' does not exist or there is not a table named '" +
            TAB_BMP_SCENARIO + "' in BMP database.");
    }
    mongoc_collection_t *sceCollection;
    sceCollection = mongoc_client_get_collection(m_conn, m_bmpDBName.c_str(), TAB_BMP_SCENARIO);
    /// Find the unique scenario name
    bson_t *query = bson_new(), *reply = bson_new();
    query = BCON_NEW("distinct", BCON_UTF8(TAB_BMP_SCENARIO), "key", FLD_SCENARIO_NAME,
                     "query", "{", FLD_SCENARIO_ID, BCON_INT32(m_id), "}");
    bson_iter_t iter, sub_iter;
    bson_error_t *err = NULL;
    if (mongoc_collection_command_simple(sceCollection, query, NULL, reply, err)) {
        //cout<<bson_as_json(reply,NULL)<<endl;
        if (bson_iter_init_find(&iter, reply, "values") &&
            (BSON_ITER_HOLDS_DOCUMENT(&iter) || BSON_ITER_HOLDS_ARRAY(&iter)) &&
            bson_iter_recurse(&iter, &sub_iter)) {
            while (bson_iter_next(&sub_iter)) {
                m_name = GetStringFromBsonIterator(&sub_iter);
                break;
            }
        } else {
            throw ModelException("BMP Scenario", "loadScenarioName",
                                 "There is not scenario existed with the ID: " + ValueToString(m_id)
                                     + " in " + TAB_BMP_SCENARIO + " table in BMP database.");
        }
    }
    bson_destroy(query);
    bson_destroy(reply);
    mongoc_collection_destroy(sceCollection);
}

void Scenario::loadBMPs() {
    vector<string>::iterator it = find(m_bmpCollections.begin(), m_bmpCollections.end(), string(TAB_BMP_INDEX));
    if (it == m_bmpCollections.end()) {
        throw ModelException("BMP Scenario", "loadScenarioName", "The BMP database '" + m_bmpDBName +
            "' does not exist or there is not a table named '" +
            TAB_BMP_INDEX + "' in BMP database.");
    }
    bson_t *query = bson_new();
    BSON_APPEND_INT32(query, FLD_SCENARIO_ID, m_id);
    //cout<<bson_as_json(query, NULL)<<endl;
    mongoc_collection_t *collection = mongoc_client_get_collection(m_conn, m_bmpDBName.c_str(), TAB_BMP_SCENARIO);
    mongoc_collection_t *collbmpidx = mongoc_client_get_collection(m_conn, m_bmpDBName.c_str(), TAB_BMP_INDEX);
    mongoc_cursor_t *cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);
    bson_error_t *err = NULL;
    if (mongoc_cursor_error(cursor, err)) {
        throw ModelException("BMP Scenario", "loadBMPs",
                             "There are no record with scenario ID: " + ValueToString(m_id));
    }
    const bson_t *info;
    while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &info)) {
        //cout<<bson_as_json(info,0)<<endl;
        bson_iter_t iter;
        int BMPID = -1;
        int subScenario = -1;
        string distribution = "";
        string collectionName = "";
        string location = "";
        if (bson_iter_init_find(&iter, info, FLD_SCENARIO_BMPID)) GetNumericFromBsonIterator(&iter, BMPID);
        if (bson_iter_init_find(&iter, info, FLD_SCENARIO_SUB)) GetNumericFromBsonIterator(&iter, subScenario);
        if (bson_iter_init_find(&iter, info, FLD_SCENARIO_DIST)) distribution = GetStringFromBsonIterator(&iter);
        if (bson_iter_init_find(&iter, info, FLD_SCENARIO_TABLE)) collectionName = GetStringFromBsonIterator(&iter);
        if (bson_iter_init_find(&iter, info, FLD_SCENARIO_LOCATION)) location = GetStringFromBsonIterator(&iter);

        int BMPType = -1;
        int BMPPriority = -1;
        bson_t *queryBMP = bson_new();
        BSON_APPEND_INT32(queryBMP, FLD_BMP_ID, BMPID);
        mongoc_cursor_t *cursor2 = mongoc_collection_find(collbmpidx, MONGOC_QUERY_NONE, 0, 0, 0, queryBMP, NULL,
                                                          NULL);
        if (mongoc_cursor_error(cursor2, err)) {
            throw ModelException("BMP Scenario", "loadBMPs",
                                 "There are no record with BMP ID: " + ValueToString(BMPID));
        }
        const bson_t *info2;
        while (mongoc_cursor_more(cursor2) && mongoc_cursor_next(cursor2, &info2)) {
            bson_iter_t sub_iter;
            if (bson_iter_init_find(&sub_iter, info2, FLD_BMP_TYPE)) GetNumericFromBsonIterator(&sub_iter, BMPType);
            if (bson_iter_init_find(&sub_iter, info2, FLD_BMP_PRIORITY)) {
                GetNumericFromBsonIterator(&sub_iter, BMPPriority);
            }
        }
        //cout<<BMPID<<","<<BMPType<<","<<distribution<<","<<parameter<<endl;
        bson_destroy(queryBMP);
        mongoc_cursor_destroy(cursor2);
        /// Combine BMPID, and SubScenario for a unique ID to identify "different" BMP
        int uniqueBMPID = BMPID * 100000 + subScenario;
        if (this->m_bmpFactories.find(uniqueBMPID) == this->m_bmpFactories.end()) {
            if (BMPID == BMP_TYPE_POINTSOURCE) {
                this->m_bmpFactories[uniqueBMPID] = new BMPPointSrcFactory(m_id, BMPID, subScenario, BMPType,
                                                                           BMPPriority, distribution,
                                                                           collectionName, location);
            }
            if (BMPID == BMP_TYPE_PLANT_MGT) {
                this->m_bmpFactories[uniqueBMPID] = new BMPPlantMgtFactory(m_id, BMPID, subScenario, BMPType,
                                                                           BMPPriority, distribution,
                                                                           collectionName, location);
            }
            if (BMPID == BMP_TYPE_AREALSOURCE) {
                this->m_bmpFactories[uniqueBMPID] = new BMPArealSrcFactory(m_id, BMPID, subScenario, BMPType,
                                                                           BMPPriority, distribution,
                                                                           collectionName, location);
            }
        }
    }
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);
    mongoc_collection_destroy(collbmpidx);
}

void Scenario::loadBMPDetail() {
    map<int, BMPFactory *>::iterator it;
    for (it = this->m_bmpFactories.begin(); it != this->m_bmpFactories.end(); it++) {
        it->second->loadBMP(this->m_conn, m_bmpDBName);
    }
}

void Scenario::Dump(string fileName) {
    ofstream fs;
    fs.open(fileName.c_str(), ios::ate);
    if (fs.is_open()) {
        Dump(&fs);
        fs.close();
    }
}

void Scenario::Dump(ostream *fs) {
    if (fs == NULL) return;

    *fs << "Scenario ID:" << this->m_id << endl;
    *fs << "Name:" << this->m_name << endl;
    *fs << "*** All the BMPs ***" << endl;
    map<int, BMPFactory *>::iterator it;
    for (it = this->m_bmpFactories.begin(); it != this->m_bmpFactories.end(); it++) {
        if (it->second != NULL) it->second->Dump(fs);
    }
}
}