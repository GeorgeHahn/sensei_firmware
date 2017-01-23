from datetime import datetime, timedelta

def format_time_str(offset_hours):
    t = datetime.now() - timedelta(hours=offset_hours)
    return t.strftime('%m%d%y%w%H%M%S%f')[0:16]
