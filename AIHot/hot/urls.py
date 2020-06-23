from django.urls import path
from .views import *

app_name = 'hot'


urlpatterns = [
    path('', index, name='index'),
    path('power/', power, name='power'),
    path('start/', heat, name='heat'),
    path('get-info/', get_info, name='get_info'),
    path('reduce-water', reduce_water, name='reduce_water'),
    path('add-water', add_water, name='add_water'),
]



