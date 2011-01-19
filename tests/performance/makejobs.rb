#!/usr/bin/env ruby

# this ruby script generates a chronologically sorted 

res = ""
input = File.read(ARGV[0])
input = input.gsub(/#.*/, "")
input.split("\n").each {|a|
  if a =~ /([^ ]*)\.\.([^ ]*) %(.*)/
    res += `git log #{$1}..#{$2} | grep '^commit' | sed 's/commit //' | sed -n '0~#{$3}p'`
  elsif a =~ /([^ ]*)\.\.([^ ]*)/
    res += `git log #{$1}..#{$2} | grep '^commit' | sed 's/commit //'`
  else
    res += `echo #{a}`
  end
}

all = `git log | grep '^commit' | sed 's/commit//' `
all.split("\n").reverse.each {|a|
  if res.match(a.strip) != nil
    puts "#{a.strip}"
  end
}

