#pragma once

#include <memory>
#include <sstream>
#include <iostream>
#include <Core/Types.h>
#include <Common/Exception.h>
#include <Parsers/Lexer.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}


/// Checks expected server and client error codes in testmode.
/// To enable it add special comment after the query: "-- { serverError 60 }" or "-- { clientError 20 }".
class TestHint
{
public:
    TestHint(bool enabled_, const String & query)
    :   enabled(enabled_)
    {
        if (!enabled_)
            return;

        String full_comment;
        Lexer lexer(query.data(), query.data() + query.size());

        for (Token token = lexer.nextToken(); !token.isEnd(); token = lexer.nextToken())
        {
            if (token.type == TokenType::Comment)
                full_comment += String(token.begin, token.begin + token.size()) + ' ';
        }

        if (!full_comment.empty())
        {
            size_t pos_start = full_comment.find('{', 0);
            if (pos_start != String::npos)
            {
                size_t pos_end = full_comment.find('}', pos_start);
                if (pos_end != String::npos)
                {
                    String hint(full_comment.begin() + pos_start + 1, full_comment.begin() + pos_end);
                    parse(hint);
                }
            }
        }
    }

    /// @returns true if it's possible to continue without reconnect
    bool checkActual(int & actual_server_error, int & actual_client_error,
                     bool & got_exception, std::unique_ptr<Exception> & last_exception) const
    {
        if (!enabled)
            return true;

        if (allErrorsExpected(actual_server_error, actual_client_error))
        {
            got_exception = false;
            last_exception.reset();
            actual_server_error = 0;
            actual_client_error = 0;
            return false;
        }

        if (lostExpectedError(actual_server_error, actual_client_error))
        {
            std::cerr << "Success when error expected. It expects server error "
                << server_error << ", client error " << client_error << "." << std::endl;
            got_exception = true;
            last_exception = std::make_unique<Exception>("Success when error expected", ErrorCodes::LOGICAL_ERROR); /// return error to OS
            return false;
        }

        return true;
    }

    int serverError() const { return server_error; }
    int clientError() const { return client_error; }

private:
    bool enabled = false;
    int server_error = 0;
    int client_error = 0;

    void parse(const String & hint)
    {
        std::stringstream ss;
        ss << hint;
        while (!ss.eof())
        {
            String item;
            ss >> item;
            if (item.empty())
                break;

            if (item == "serverError")
                ss >> server_error;
            else if (item == "clientError")
                ss >> client_error;
        }
    }

    bool allErrorsExpected(int actual_server_error, int actual_client_error) const
    {
        return (server_error || client_error) && (server_error == actual_server_error) && (client_error == actual_client_error);
    }

    bool lostExpectedError(int actual_server_error, int actual_client_error) const
    {
        return (server_error && !actual_server_error) || (client_error && !actual_client_error);
    }
};

}
