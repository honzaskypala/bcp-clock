# bestcoastpairings.com integration
# (c) 2025 Honza Skýpala
# WTFPL license applies

import re
import time
import math
from datetime import strptime
from random import randint
import requests

class Event:
    def __init__(self, event_id):
        self.timer = {}
        self.init_string = event_id
        # if event url passed instead of id, extract id
        match = re.search(r"^(?:https://www\.bestcoastpairings\.com/(?:organize/)?)?(?:event/)?([^/?]+)", self.init_string)
        if match:
            self.event_id = match.group(1)
            self.refresh()
        else:
            raise ValueError("Invalid BCP event id.")

    def refresh(self):
        """ Refresh event data from BCP API """
        baseurl = f"https://newprod-api.bestcoastpairings.com/v1/events/{self.event_id}"
        headers = {"client-id": "web-app"}
        # get event overview from BCP API
        response = requests.get(f"{baseurl}/overview", headers=headers)
        if response.status_code == 200:
            self.overview = response.json()
            if self.overview["status"]["started"] and not self.overview["status"]["ended"]:
                # if event is ongoing, get timer info for current round
                current_round = self.overview['status']['currentRound']
                response = requests.get(f"{baseurl}/timer?round={current_round}", headers=headers)
                if response.status_code == 200:
                    self.timer[current_round] = response.json()

    @property
    def remaining_seconds(self):
        """ Returns remaining time in seconds for the current round. Negative value means overtime. """
        return strptime(self.timer[self.overview['status']['currentRound']]['endTime']) - math.floor(time.time())
    
    @property
    def remaining_time_str(self):
        """ Returns remaining time as formatted string MM:SS or HH:MM:SS. Negative value means overtime. """
        sign = ""
        remaining = self.remaining_seconds
        if remaining < 0:
            sign = "-"
            remaining = abs(remaining)
        hours = remaining // 3600
        minutes = (remaining % 3600) // 60
        seconds = remaining % 60
        if self.timer[self.overview['status']['currentRound']]['timerLength'] > 3600:
            return f"{sign}{hours:02d}:{minutes:02d}:{seconds:02d}"
        elif hours:
            return f"{sign}XX:XX"
        else:
            return f"{sign}{minutes:02d}:{seconds:02d}"
