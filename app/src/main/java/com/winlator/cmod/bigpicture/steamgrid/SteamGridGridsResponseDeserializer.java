package com.winlator.cmod.bigpicture.steamgrid;

import com.google.gson.JsonArray;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;

import java.lang.reflect.Type;
import java.util.ArrayList;

public class SteamGridGridsResponseDeserializer implements JsonDeserializer<SteamGridGridsResponse> {

    @Override
    public SteamGridGridsResponse deserialize(JsonElement json, Type typeOfT, JsonDeserializationContext context) throws JsonParseException {
        SteamGridGridsResponse response = new SteamGridGridsResponse();
        JsonObject jsonObject = json.getAsJsonObject();

        // Null check for success field
        if (jsonObject.has("success") && !jsonObject.get("success").isJsonNull()) {
            response.success = jsonObject.get("success").getAsBoolean();
        } else {
            response.success = false;  // Default value if not present
        }

        // Null check for page, total, and limit fields
        if (jsonObject.has("page") && !jsonObject.get("page").isJsonNull()) {
            response.page = jsonObject.get("page").getAsInt();
        } else {
            response.page = 0;  // Default value if not present
        }

        if (jsonObject.has("total") && !jsonObject.get("total").isJsonNull()) {
            response.total = jsonObject.get("total").getAsInt();
        } else {
            response.total = 0;  // Default value if not present
        }

        if (jsonObject.has("limit") && !jsonObject.get("limit").isJsonNull()) {
            response.limit = jsonObject.get("limit").getAsInt();
        } else {
            response.limit = 0;  // Default value if not present
        }

        // Check if data is an array or an object
        JsonElement dataElement = jsonObject.get("data");
        if (dataElement != null && dataElement.isJsonArray()) {
            response.data = new ArrayList<>();
            JsonArray gridsArray = dataElement.getAsJsonArray();
            for (JsonElement element : gridsArray) {
                SteamGridGridsResponse.Grid grid = context.deserialize(element, SteamGridGridsResponse.Grid.class);
                response.data.add(grid);
            }
        } else if (dataElement != null && dataElement.isJsonObject()) {
            // Handle case where data is a single object
            response.data = new ArrayList<>();
            SteamGridGridsResponse.Grid grid = context.deserialize(dataElement, SteamGridGridsResponse.Grid.class);
            response.data.add(grid);
        }

        return response;
    }
}

