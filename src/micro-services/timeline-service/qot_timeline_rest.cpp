/*
 * @file qot_timeline_rest.cpp
 * @brief Timeline REST API Interface to the Coordination Service
 * @author Anon D'Anon
 *
 * Copyright (c) Anon, 2018.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * Based on: https://mariusbancila.ro/blog/2017/11/19/revisited-full-fledged-client-server-example-with-c-rest-sdk-2-10/
 */

// Local Header
#include "qot_timeline_rest.hpp"

#include <cpprest/json.h>

// StdLib Headers
#include <iostream>

// Using the qot_core namespace
using namespace qot_core;

/* Private Helper Functions */
void display_json(json::value const & jvalue, utility::string_t const & prefix)
{
   std::cout << prefix << jvalue.serialize() << std::endl;
}
 
pplx::task<http_response> make_task_request(http_client & client, method mtd, std::string path, json::value const & jvalue)
{
   return (mtd == methods::GET || mtd == methods::HEAD) ?
      client.request(mtd, path) :
      client.request(mtd, path, jvalue);
}
 
json::value make_request(http_client & client, method mtd, std::string path, json::value const & jvalue)
{
   auto answer = json::value::object();

   make_task_request(client, mtd, path, jvalue)
      .then([](http_response response)
      {
         std::cout << "TimelineRestInterface: Received response status code: " << response.status_code() << std::endl;
         if (response.status_code() == status_codes::OK)
         {
            return response.extract_json();
         }
         return pplx::task_from_result(json::value());
      })
      .then([&answer](pplx::task<json::value> previousTask)
      {
         try
         {
            display_json(previousTask.get(), "R: ");
            answer = previousTask.get();
         }
         catch (http_exception const & e)
         {
            std::cout << e.what() << std::endl;
         }
      })
      .wait();

   return answer;
}

/* Constructor and Destuctor */
TimelineRestInterface::TimelineRestInterface(std::string host)
 : host_url(host), client(host)
{
   // Initializattion can be done in this function
}

TimelineRestInterface::~TimelineRestInterface()
{
   // De-initialization can be done in this function
}

/* Public Functions */
std::vector<std::string> TimelineRestInterface::get_timelines()
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::vector<std::string> timeline_vector;
   std::cout << "TimelineRestInterface: GET (get all timelines)\n";
   auto answer = make_request(new_client, methods::GET, std::string("/api/service/timelines/"), json::value::null());

   // Unpack the timelines
   for(auto array_iter = answer.as_array().cbegin(); array_iter != answer.as_array().cend(); ++array_iter)
   { 
      for(auto iter = array_iter->as_object().cbegin(); iter != array_iter->as_object().cend(); ++iter)
      {
         const utility::string_t &key = iter->first;
         const json::value &value = iter->second;

         // Append the timeline name to the timeline vector
         if(key.compare("name") == 0)
         {
            timeline_vector.push_back(value.as_string());
            break;
         }

         // std::cout << "Key: " << key << ", Value: " << value.serialize() << std::endl;
      }
   }

   return timeline_vector;
}

int TimelineRestInterface::post_timeline(std::string timeline_uuid)
{
   // Create a JSON to send to the REST Coordination Service
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   auto timeline = json::value::object();
   timeline["id"] = json::value::number(0);                 // ID Defaults to 0 (Coordination Service will generate an ID)
   timeline["name"] = json::value::string(timeline_uuid);

   // Make the POST Request
   std::cout << "TimelineRestInterface: POST (register a new timeline): " << timeline_uuid << std::endl;
   make_request(new_client, methods::POST, std::string("/api/service/timelines/"), timeline);

   return 0;
}

int TimelineRestInterface::delete_timeline(std::string timeline_uuid)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid;
   std::cout << "TimelineRestInterface: DELETE (delete a timeline): " << timeline_uuid << std::endl;
   make_request(new_client, methods::DEL, path, json::value::null());
   return 0;
}

std::vector<qot_node_phy_t> TimelineRestInterface::get_timeline_nodes(std::string timeline_uuid)
{
   std::vector<qot_node_phy_t> node_vector;
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::vector<std::string> timeline_nodes;
   std::string path = "/api/service/timelines/" + timeline_uuid;
   std::cout << "TimelineRestInterface: GET (get timeline nodes): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   // Unpack the timelines
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   { 
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the nodes
      if (key.compare("nodes") == 0)
      {
         for(auto array_iter = value.as_array().cbegin(); array_iter != value.as_array().cend(); ++array_iter)
         {
            qot_node_phy_t node;
            const utility::string_t &key_in = iter->first;
            const json::value &value_in = iter->second;
            for(auto iter_in = array_iter->as_object().cbegin(); iter_in != array_iter->as_object().cend(); ++iter_in)
            {
               if (key_in.compare("name") == 0)
               {
                  node.name = value.as_string();
               }
               else if (key_in.compare("accuracy") == 0)
               {
                  node.accuracy_ns = value.as_integer();
               }
               else if (key_in.compare("resolution") == 0)
               {
                  node.resolution_ns = value.as_integer();
               }
               node_vector.push_back(node);
            }
         }
         break;
      }
   }
   return node_vector;
}

// Get timeline ID on the coordination service
int TimelineRestInterface::get_timeline_coord_id(std::string timeline_uuid)
{
   int id = -1;
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::vector<std::string> timeline_nodes;
   std::string path = "/api/service/timelines/" + timeline_uuid;
   std::cout << "TimelineRestInterface: GET (get timeline coordination id): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   // Unpack the timelines
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   { 
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the timeline coordination id
      if (key.compare("id") == 0)
      {
         id = value.as_integer();
         break;
      }
      
   }
   return id;
}

// Get the Timeline Metadata
std::string TimelineRestInterface::get_timeline_metadata(std::string timeline_uuid)
{
   std::string meta_data = "NULL"; 
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid;
   std::cout << "TimelineRestInterface: GET (get timeline meta data): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   // Unpack the timelines
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   { 
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the meta data
      if (key.compare("meta_data") == 0)
      {
         meta_data = value.as_string();
         break;
      }
   }
   return meta_data;
}

// Update the timeline meta data
int TimelineRestInterface::put_timeline_metadata(std::string timeline_uuid, std::string meta_data)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   auto timeline = json::value::object();
   timeline["id"] = json::value::number(0);                 // ID Defaults to 0 (Coordination Service will generate an ID)
   timeline["name"] = json::value::string(timeline_uuid);
   timeline["meta_data"] = json::value::string(meta_data);

   std::string path = "/api/service/timelines/" + timeline_uuid;
   std::cout << "TimelineRestInterface: PUT (update timeline meta data): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::PUT, path, timeline);
   return 0;
}

int TimelineRestInterface::get_timeline_num_nodes(std::string timeline_uuid)
{  
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   int num_nodes = 0;
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes";
   std::cout << "TimelineRestInterface: GET (get timeline number of nodes): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   {
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the nodes
      if (key.compare("num_nodes") == 0)
      {
         num_nodes = value.as_integer();
      }
   }
   return num_nodes;
}

int TimelineRestInterface::post_node(std::string timeline_uuid, std::string node_uuid, unsigned long long accuracy_ns, unsigned long long resolution_ns)
{
   // Create a JSON to send to the REST Coordination Service
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   auto node = json::value::object();
   node["id"] = json::value::number(0);                 // ID Defaults to 0 (Coordination Service will generate an ID)
   node["name"] = json::value::string(node_uuid);
   node["timeline_name"] = json::value::string(timeline_uuid);
   node["accuracy"] = json::value::number(uint64_t(accuracy_ns));
   node["resolution"] = json::value::number(uint64_t(resolution_ns));

   // Request path
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes";

   // Make the POST Request
   std::cout << "TimelineRestInterface: POST (register a new node: " << node_uuid << " on timeline: " << timeline_uuid << ")" << std::endl;
   std::cout << "TimelineRestInterface: accuracy = " << accuracy_ns << ", resolution_ns = " << resolution_ns << std::endl;
   make_request(new_client, methods::POST, path, node);
   return 0;
}

int TimelineRestInterface::delete_node(std::string timeline_uuid, std::string node_uuid)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes/" + node_uuid;
   std::cout << "TimelineRestInterface: DELETE (delete a node: " << node_uuid << " on timeline: " << timeline_uuid << ")" << std::endl;
   make_request(new_client, methods::DEL, path, json::value::null());
   return 0;
}

int TimelineRestInterface::get_node(std::string timeline_uuid, std::string node_uuid, unsigned long long &accuracy_ns, unsigned long long &resolution_ns)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes/" + node_uuid;
   std::cout << "TimelineRestInterface: GET (get info node: " << node_uuid << " on timeline: " << timeline_uuid << ")" << std::endl;
   auto answer = make_request(new_client, methods::GET, path, json::value::null());
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   {
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the nodes
      if (key.compare("accuracy") == 0)
      {
         accuracy_ns = value.as_integer();
      }
      else if (key.compare("resolution") == 0)
      {
         resolution_ns = value.as_integer();
      }
   }
   return 0;
}

std::string TimelineRestInterface::get_node_ip(std::string timeline_uuid, std::string node_uuid)
{
   std::string ip_address = "NULL";
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes/" + node_uuid;
   std::cout << "TimelineRestInterface: GET (get IP address for node: " << node_uuid << " on timeline: " << timeline_uuid << ")" << std::endl;
   auto answer = make_request(new_client, methods::GET, path, json::value::null());
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   {
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the nodes
      if (key.compare("ip") == 0)
      {
         ip_address = value.as_string();
      }
   }
   return ip_address;
}

int TimelineRestInterface::put_node(std::string timeline_uuid, std::string node_uuid, unsigned long long accuracy_ns, unsigned long long resolution_ns)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   auto qot = json::value::object();
   qot["accuracy"] = json::value::number(uint64_t(accuracy_ns));
   qot["resolution"] = json::value::number(uint64_t(resolution_ns));
   std::string path = "/api/service/timelines/" + timeline_uuid + "/nodes/" + node_uuid;
   std::cout << "TimelineRestInterface: PUT (update a node: " << node_uuid << " on timeline: " << timeline_uuid << ")" << std::endl;
   std::cout << "TimelineRestInterface: accuracy = " << accuracy_ns << ", resolution_ns = " << resolution_ns << std::endl;
   make_request(new_client, methods::PUT, path, qot);
   return 0;
}

int TimelineRestInterface::get_timeline_qot(std::string timeline_uuid, unsigned long long &accuracy_ns, unsigned long long &resolution_ns)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/qot";
   std::cout << "TimelineRestInterface: GET (get timeline qot: " << timeline_uuid << ")" << std::endl;
   auto answer = make_request(new_client, methods::GET, path, json::value::null());
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   {
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Extract the nodes
      if (key.compare("accuracy") == 0)
      {
         accuracy_ns = value.as_integer();
      }
      else if (key.compare("resolution") == 0)
      {
         resolution_ns = value.as_integer();
      }
   }
   return 0;
}

std::vector<qot_server_t> TimelineRestInterface::get_timeline_servers(std::string timeline_uuid)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::vector<qot_server_t> servers;
   std::string path = "/api/service/timelines/" + timeline_uuid + "/servers";
   std::cout << "TimelineRestInterface: GET (get timeline servers): " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   if (answer.is_array())
   {
      std::cout << "TimelineRestInterface: GET (get timeline servers) returned data\n";
      // Unpack the servers
      for(auto array_iter = answer.as_array().cbegin(); array_iter != answer.as_array().cend(); ++array_iter)
      { 
         qot_server_t server;
         server.hostname = ""; // Set to an empty string
         for(auto iter = array_iter->as_object().cbegin(); iter != array_iter->as_object().cend(); ++iter)
         {
            const utility::string_t &key = iter->first;
            const json::value &value = iter->second;

            // Append the timeline name to the timeline vector
            if(key.compare("name") == 0)
            {
               server.hostname = value.as_string();
            }
            else if (key.compare("stratum") == 0)
            {
               if (value.is_null())
                  server.stratum = 3; // Needs to be changed
               else
                  server.stratum = value.as_integer();
            }
            else if (key.compare("server_type") == 0)
            {
               server.type = value.as_string();
            }

            // std::cout << "Key: " << key << ", Value: " << value.serialize() << std::endl;
         }
         servers.push_back(server);
      }
   }
   return servers;
} 

int TimelineRestInterface::post_timeline_server(std::string timeline_uuid, qot_server_t &server)
{
   // Create a JSON to send to the REST Coordination Service
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   auto server_json = json::value::object();
   server_json["name"] = json::value::string(server.hostname);                 
   server_json["server_type"] = json::value::string(server.type);
   server_json["stratum"] = json::value::number(server.stratum);
   // server_json["root_dispersion"] = json::value::number(uint64_t(accuracy_ns));
   // server_json["root_delay"] = json::value::number(uint64_t(resolution_ns));

   // Request path
   std::string path = "/api/service/timelines/" + timeline_uuid + "/servers";

   // Make the POST Request
   std::cout << "TimelineRestInterface: POST (register a new server: " << server.hostname << " on timeline: " << timeline_uuid << ")" << std::endl;
   std::cout << "TimelineRestInterface: Server Info is: stratum " << server_json["stratum"] << ", type " << server_json["server_type"] << std::endl;
   make_request(new_client, methods::POST, path, server_json);
   return 0;
}

int TimelineRestInterface::get_timeline_server_info(std::string timeline_uuid, qot_server_t &server)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/servers/" + server.hostname;
   std::cout << "TimelineRestInterface: GET (get timeline server " << server.hostname << " ) on : " << timeline_uuid << "\n";
   auto answer = make_request(new_client, methods::GET, path, json::value::null());

   // Unpack the server
   for(auto iter = answer.as_object().cbegin(); iter != answer.as_object().cend(); ++iter)
   {
      const utility::string_t &key = iter->first;
      const json::value &value = iter->second;

      // Append the timeline name to the timeline vector
      if(key.compare("name") == 0)
      {
         server.hostname = value.as_string();
      }
      else if (key.compare("stratum") == 0)
      {
         server.stratum = value.as_integer();
      }
      else if (key.compare("server_type") == 0)
      {
         server.type = value.as_string();
      }

      // std::cout << "Key: " << key << ", Value: " << value.serialize() << std::endl;
   }  
   return 0;
}

int TimelineRestInterface::delete_timeline_server(std::string &timeline_uuid, std::string &server_name)
{
   http_client new_client(host_url);                        // Creating a new client to ensure message goes through (Bug in C++ Rest SDK)
   std::string path = "/api/service/timelines/" + timeline_uuid + "/servers/" + server_name;
   std::cout << "TimelineRestInterface: DELETE (delete a server: " << server_name << " on timeline: " << timeline_uuid << ")" << std::endl;
   make_request(new_client, methods::DEL, path, json::value::null());
   return 0;
}
