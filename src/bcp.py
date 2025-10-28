# BCP integration
# (c) 2025 Honza Skýpala
# WTFPL license applies

import re
import time
import math
from datetime import strptime
import json
from random import randint

class Event:
    def __init__(self, event_id):
        self._sample_init()
        self.init_string = event_id
        match = re.search(r"^(?:https://www\.bestcoastpairings\.com/(?:organize/)?)?(?:event/)?([^/?]+)", self.init_string)
        if match:
            self.event_id = match.group(1)
            self.refresh()
        else:
            raise ValueError("Invalid BCP event id.")

    def refresh(self):
        self.overview = json.loads(self._sample_overview())
        if self.overview["status"]["started"] and not self.overview["status"]["ended"]:
            self.timer = json.loads(self._sample_timer())
    
    @property
    def remaining_seconds(self):
        return strptime(self.timer["endTime"]) - math.floor(time.time())
    
    @property
    def remaining_time_str(self):
        sign = ""
        remaining = self.remaining_seconds
        if remaining < 0:
            sign = "-"
            remaining = abs(remaining)
        hours = remaining // 3600
        minutes = (remaining % 3600) // 60
        seconds = remaining % 60
        if self.timer["timerLength"] > 3600:
            return f"{sign}{hours:02d}:{minutes:02d}:{seconds:02d}"
        elif hours:
            return f"{sign}XX:XX"
        else:
            return f"{sign}{minutes:02d}:{seconds:02d}"

    def _sample_init(self):
        self._timer_length = 3600
        start_time_sec = math.floor(time.time())
        t = time.gmtime(start_time_sec)
        self._start_time_str = f"{t[0]:04d}-{t[1]:02d}-{t[2]:02d}T{t[3]:02d}:{t[4]:02d}:{t[5]:02d}Z"
        end_time_sec = start_time_sec + self._timer_length
        t = time.gmtime(end_time_sec)
        self._end_time_str = f"{t[0]:04d}-{t[1]:02d}-{t[2]:02d}T{t[3]:02d}:{t[4]:02d}:{t[5]:02d}Z"

    def _sample_overview(self):
        started = "true"
        ended = "false"
        number_of_rounds = 5
        current_round = randint(1, number_of_rounds)
        return f'{{"id":"19u0tXk99XHs","updated_at":"2025-10-05T15:51:51.101Z","created_at":"2025-07-17T15:41:25.840Z","owner":{{"id":"ybn9TY71La","firstName":"Honza","lastName":"Skýpala"}},"eventUsers":[{{"id":"ybn9TY71La","firstName":"Honza","lastName":"Skýpala","role":{{"id":"DefaultRole","name":"Tournament Organizer","orderNum":1}}}}],"name":"Czech Major 2025","description":"We invite you to a prestigeous two-days Warhammer Underworlds tournament in the Czech Republic for this year. As the Prague Open Wargaming Event is not happening in 2025, we are not part of a large event this time. Nevertheless, we decided to organize the whole tournament ourselves and chose a new name: Czech Major.\n\n**Term**: 4-5th October 2025. Due to the term it is ideal for Worlds contenders to train their decks and strategies, but of course the event is open to everyone.\n\n**Location**: The Grail, Viktora Huga 287/5, Prague, Czech Republic\n\n**Format**: Nemesis Bo3, first day 4 rounds swiss pairing, second day elimination bracket for top 8 and side tournament for the rest. We will use state of the game valid on 30th September including pontential new products and FAQ released by that day. Anything new released or published on 1st October or later will not be valid at the event. Glory difference will be capped at 7 points max for a single game. Detailed rulepack to be released soon.\n\n**Prizes**: custom glass trophy and other prizes\n\n**Registration**: payment in advance €25 Early Bird price until 31st August, standard price of €33 from 1st September. For foreign players we offer payment using PayPal, Revolut or Bitcoin Lightning. Please contact me at [honza@honza.info](mailto:honza@honza.info) for payment details.\n\n**Rulespack**: [https://drive.google.com/file/d/1PT9IEOt9_BrNe198Z7El7sF-M1g0d8uB/view?usp=sharing](https://drive.google.com/file/d/1PT9IEOt9_BrNe198Z7El7sF-M1g0d8uB/view?usp=sharing)\n\nSee you in Prague!","format":{{"teamEvent":false,"doublesEvent":false,"leagueEvent":false,"boardGameEvent":false,"bracketPairings":true}},"sponsored":false,"featured":false,"isOnlineEvent":false,"dates":{{"start":"2025-10-04T07:00:00.000Z","end":"2025-10-05T16:00:00.000Z"}},"status":{{"started":{started},"ended":{ended},"currentRound":{current_round},"numberOfRounds":{number_of_rounds}}},"flags":{{"hideLists":false,"hidePlacings":true,"hideRoster":false,"hidePlayerCount":false,"listsAtCheckin":true,"disableCheckin":false,"listsLocked":true,"listSubmissionLocked":false,"passwordlessScoring":true}},"ticketing":{{"useTicketing":false,"numTickets":0,"shippingDetails":{{"requested":false,"mandatory":false,"description":""}},"price":0,"currency":"usd","chargeType":"stripe","privateEvent":false,"playerPaysFees":false,"isFull":true}},"playerCounts":{{"checkedIn":18,"notCheckedIn":6,"dropped":8,"active":16,"total":24}},"countLabel":"Total Players Registered","countString":"24","gameSystem":{{"id":"Z8rcM0GCLw","name":"Warhammer Underworlds","code":9,"manufacturer":"Games Workshop","usesSystemId":false,"systemIdRequired":false,"systemIdName":null,"hasFactions":true,"factionLabel":"Warband","subFactionLabel":"Sub Faction","supportsCharacters":false,"characterLabel":"Sub Faction"}},"eventImage":{{"photoUrl":"https://d17plg9ep1ptvd.cloudfront.net/2025-07-17/RrRGkv21Y5mE.png"}},"leagues":[{{"id":"DV9t3hNMQjJn","name":"2025 Warhammer Underworlds ITC","submitted":false}}],"location":{{"name":"The Grail","point":{{"latitude":50.0753057,"longitude":14.4027987}},"state":"Hlavní město Praha","zip":"150 00","country":"Česko","streetNum":"5","streetName":"Viktora Huga","formatted_address":"The Grail, Viktora Huga, Praha 5-Smíchov 150 00, Česko"}},"isTO":false,"following":false,"externalUrl":""}}'

    def _sample_timer(self):
        """
        self._end_time_str = "2025-10-11T17:39:23Z"
        """
        return f'{{"timerLength":{self._timer_length},"startTime":"{self._start_time_str}","endTime":"{self._end_time_str}"}}'

