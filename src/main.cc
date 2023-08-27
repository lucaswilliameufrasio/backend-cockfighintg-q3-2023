#include <drogon/drogon.h>

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

using namespace std;
using namespace drogon;

HttpResponsePtr makeBadRequestResponse(string message)
{
    Json::Value json;
    json["message"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k400BadRequest);
    return resp;
}

HttpResponsePtr makeNotFoundResponseResponse(string message)
{
    Json::Value json;
    json["message"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k404NotFound);
    return resp;
}

HttpResponsePtr makeUnprocessableContentResponseResponse(string message)
{
    Json::Value json;
    json["message"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k422UnprocessableEntity);
    return resp;
}

HttpResponsePtr makeFailedResponse()
{
    Json::Value json;
    json["message"] = "Manda o negócio direito porque deu bom não";
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k500InternalServerError);
    return resp;
}

void logInfo(const string message)
{
    cout << message << endl;
}

void logException(const char *message)
{
    cerr << "error:" << message << endl;
}

const int MAX_VALID_YR = 9999;
const int MIN_VALID_YR = 1800;

bool isLeap(int year)
{
    return bool(((year % 4 == 0) &&
                 (year % 100 != 0)) ||
                (year % 400 == 0));
}

bool isDateValid(string date)
{
    if (date.size() != 10) {
        return false;
    }

    vector<int> partsOfDate;

    stringstream dateStream(date);

    string split;
    while (getline(dateStream, split, '-'))
    {
        try {
            partsOfDate.push_back(stoi(split));
        } catch(exception &e) {
            break;
        }
    }

    if (partsOfDate.size() != 3) {
        return false;
    }

    auto day = partsOfDate.at(2);
    auto month = partsOfDate.at(1);
    auto year = partsOfDate.at(0);

    if (year > MAX_VALID_YR ||
        year < MIN_VALID_YR)
        return false;
    if (month < 1 || month > 12)
        return false;
    if (day < 1 || day > 31)
        return false;

    if (month == 2)
    {
        if (isLeap(year))
            return (day <= 29);
        else
            return (day <= 28);
    }

    if (month == 4 || month == 6 ||
        month == 9 || month == 11)
        return (day <= 30);

    return true;
}

int main()
{
    const auto port(atoi(getenv("PORT")));

    const auto dbMaxConnections(atol(getenv("DB_MAX_CONNECTIONS")));

    logInfo("Starting backend-cockfighting-api server on http://localhost:" + to_string(port));

    const char *dbHost = getenv("DB_HOST");
    const auto dbPort(atoi(getenv("DB_PORT")));
    const char *dbName = getenv("DB_NAME");
    const char *dbUser = getenv("DB_USER");
    const char *dbPassword = getenv("DB_PASSWORD");

    app()
        .addListener("0.0.0.0", port)
        .setThreadNum(16)
        .setIdleConnectionTimeout(10000);

    app().createDbClient("postgresql", dbHost, dbPort, dbName, dbUser, dbPassword, dbMaxConnections);

    app().registerHandler("/health-check",
                          [](const HttpRequestPtr &req,
                             function<void(const HttpResponsePtr &)> &&callback)
                          {
                              Json::Value json;
                              json["message"] = "ok";
                              auto resp = HttpResponse::newHttpJsonResponse(json);
                              callback(resp);
                          })
        .registerHandler("/pessoas",
                         [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)> &&callback)
                         {
                             auto jsonPtr = req->jsonObject();
                             if (jsonPtr == nullptr)
                             {
                                 callback(makeFailedResponse());
                                 return;
                             }

                             auto nickname = jsonPtr->get("apelido", "").as<string>();
                             auto name = jsonPtr->get("nome", "").as<string>();
                             auto birthDate = jsonPtr->get("nascimento", "").as<string>();
                             auto stacksJson = jsonPtr->get("stack", "");

                             if (nickname.size() == 0)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'apelido' parameter should not be empty"));
                                 return;
                             }

                             if (name.size() == 0)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'nome' parameter should not be empty"));
                                 return;
                             }

                             if (birthDate.size() == 0)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'nascimento' parameter should not be empty"));
                                 return;
                             }

                             if (stacksJson.size() == 0)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'stack' parameter should not be empty"));
                                 return;
                             }

                             if (nickname.size() > 32)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'apelido' parameter length should be less than 32 characters"));
                                 return;
                             }

                             if (name.size() > 100)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'apelido' parameter length should be less than 32 characters"));
                                 return;
                             }

                             if (birthDate.size() == 0 || !isDateValid(birthDate))
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'nascimento' parameter is not a valid date of format 'YYYY-MM-DD'"));
                                 return;
                             }

                             string stacks;
                             int invalidStack = 0;

                             try
                             {
                                 stacks.reserve(stacksJson.size());

                                 for (int i = 0; i < int(stacksJson.size()); ++i)
                                 {
                                     if (stacksJson[i].isNull() || stacksJson[i].isString() == 0)
                                     {
                                         invalidStack = 1;
                                         break;
                                     }

                                     string stack = stacksJson[i].asString();

                                     if (stack.size() > 32)
                                     {
                                         invalidStack = 2;
                                         break;
                                     }

                                     if (i > 0)
                                     {
                                         stacks = stacks + ",";
                                     }

                                     stacks = stacks + stack;
                                 }
                             }
                             catch (const exception &e)
                             {
                                 logException(e.what());

                                 callback(makeFailedResponse());
                                 return;
                             }

                             if (invalidStack == 2)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'stack' parameter should have items with 32 characters or less"));
                                 return;
                             }

                             if (invalidStack == 1)
                             {
                                 callback(makeUnprocessableContentResponseResponse("The 'stack' parameter should have only strings"));
                                 return;
                             }

                             auto clientPtr = drogon::app().getDbClient();

                            //  auto personFound = clientPtr->execSqlAsyncFuture("SELECT id FROM people WHERE people.nickname = $1;", nickname);

                             Json::Value json = Json::objectValue;

                            //  try
                            //  {
                            //      auto result = personFound.get();

                            //      if (result.size() > 0)
                            //      {
                            //          callback(makeUnprocessableContentResponseResponse("This person already exists."));
                            //          return;
                            //      }
                            //  }
                            //  catch (const drogon::orm::DrogonDbException &e)
                            //  {
                            //      logException(e.base().what());

                            //      callback(makeFailedResponse());
                            //      return;
                            //  }

                             const std::string uuid = boost::uuids::to_string(boost::uuids::random_generator()());

                             auto f = clientPtr->execSqlAsyncFuture("\
                                INSERT INTO people (id, nickname, name, birth_date, stack) \
                                VALUES ($1, $2, $3, $4, $5) \
                                ON CONFLICT DO NOTHING \
                                RETURNING id; \
                                ", uuid, nickname, name, birthDate, stacks);

                             string id = "";

                             try
                             {
                                 auto result = f.get();

                                 if (result.size() == 0)
                                 {
                                     callback(makeUnprocessableContentResponseResponse("This person already exists."));
                                     return;
                                 }

                                 id = result[0]["id"].as<string>();
                                 json["id"] = id;
                             }
                             catch (const drogon::orm::DrogonDbException &e)
                             {
                                 logException(e.base().what());

                                 callback(makeFailedResponse());
                                 return;
                             }

                             string location = "/pessoas/" + id;

                             auto resp = HttpResponse::newHttpJsonResponse(json);
                             resp->addHeader("Location", location);
                             resp->setStatusCode(drogon::HttpStatusCode::k201Created);
                             callback(resp);
                         },
                         {Post})
        .registerHandler("/pessoas/{id}",
                         [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)> &&callback,
                            const string &id)
                         {
                             if (id.size() != 36)
                             {
                                 callback(makeNotFoundResponseResponse("This person do not exist."));
                                 return;
                             }

                             auto clientPtr = drogon::app().getDbClient();

                             auto f = clientPtr->execSqlAsyncFuture("SELECT people.id, people.nickname, people.name, people.birth_date, people.stack FROM people WHERE people.id = $1;", id);

                             try
                             {
                                 auto result = f.get();

                                 if (result.size() == 0)
                                 {
                                     callback(makeNotFoundResponseResponse("This person do not exist."));
                                     return;
                                 }

                                 Json::Value json;

                                 stringstream stackStream(result[0]["stack"].as<string>());

                                 string split;
                                 int position = 0;
                                 while (getline(stackStream, split, ','))
                                 {
                                     json["stack"][position] = split;
                                     position++;
                                 }

                                 json["id"] = result[0]["id"].as<string>();
                                 json["apelido"] = result[0]["nickname"].as<string>();
                                 json["nome"] = result[0]["name"].as<string>();
                                 json["nascimento"] = result[0]["birth_date"].as<string>();

                                 auto resp = HttpResponse::newHttpJsonResponse(json);
                                 resp->setStatusCode(drogon::HttpStatusCode::k200OK);
                                 callback(resp);

                                 return;
                             }
                             catch (const drogon::orm::DrogonDbException &e)
                             {
                                 logException(e.base().what());

                                 callback(makeFailedResponse());
                                 return;
                             }
                         },
                         {Get})
        .registerHandler("/pessoas?t={search}",
                         [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)> &&callback,
                            const string &search)
                         {

                             if (search.empty())
                             {
                                 callback(makeBadRequestResponse("The query parameter 't' is required"));
                                 return;
                             }

                             auto clientPtr = drogon::app().getDbClient();

                             auto f = clientPtr->execSqlAsyncFuture("SELECT people.id, people.nickname, people.name, people.birth_date, people.stack FROM people WHERE LOWER(people.name || ' ' || people.nickname || ' ' || people.stack) LIKE LOWER($1) LIMIT 50;", "%" + search + "%");

                             try
                             {
                                 auto result = f.get();

                                 Json::Value json = Json::arrayValue;

                                 if (result.size() == 0)
                                 {
                                     auto resp = HttpResponse::newHttpJsonResponse(json);
                                     resp->setStatusCode(drogon::HttpStatusCode::k200OK);
                                     callback(resp);
                                     return;
                                 }

                                 int i = 0;
                                 for (auto row : result)
                                 {
                                     stringstream stackStream(result[i]["stack"].as<string>());

                                     string split;
                                     int position = 0;
                                     while (getline(stackStream, split, ','))
                                     {
                                         json[i]["stack"][position] = split;
                                         position++;
                                     }

                                     json[i]["id"] = result[i]["id"].as<string>();
                                     json[i]["apelido"] = result[i]["nickname"].as<string>();
                                     json[i]["nome"] = result[i]["name"].as<string>();
                                     json[i]["nascimento"] = result[i]["birth_date"].as<string>();
                                     i++;
                                 }

                                 auto resp = HttpResponse::newHttpJsonResponse(json);
                                 resp->setStatusCode(drogon::HttpStatusCode::k200OK);
                                 callback(resp);

                                 return;
                             }
                             catch (const drogon::orm::DrogonDbException &e)
                             {
                                 logException(e.base().what());

                                 callback(makeFailedResponse());
                                 return;
                             }
                         },
                         {Get})
        .registerHandler("/contagem-pessoas",
                         [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)> &&callback)
                         {
                             auto clientPtr = drogon::app().getDbClient();

                             auto f = clientPtr->execSqlAsyncFuture("SELECT COUNT(*) AS count FROM people;");

                             try
                             {
                                 auto result = f.get();

                                 Json::Value json = Json::objectValue;

                                 if (result.size() == 0)
                                 {
                                     json["count"] = 0;
                                     auto resp = HttpResponse::newHttpJsonResponse(json);
                                     resp->setStatusCode(drogon::HttpStatusCode::k200OK);
                                     callback(resp);
                                     return;
                                 }

                                 json["count"] = result[0]["count"].as<int>();

                                 auto resp = HttpResponse::newHttpJsonResponse(json);
                                 resp->setStatusCode(drogon::HttpStatusCode::k200OK);
                                 callback(resp);

                                 return;
                             }
                             catch (const drogon::orm::DrogonDbException &e)
                             {
                                 logException(e.base().what());

                                 callback(makeFailedResponse());
                                 return;
                             }
                         },
                         {Get})

        .run();

    return 0;
}
